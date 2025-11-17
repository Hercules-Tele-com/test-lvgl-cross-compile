#include "socketcan.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>

SocketCANData* socketcan_init(const char* interface) {
    SocketCANData* data = new SocketCANData();
    strncpy(data->interface, interface, sizeof(data->interface) - 1);

    // Automatically bring up CAN interface with 500 kbps bitrate
    printf("[SocketCAN] Bringing up interface %s...\n", interface);
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "ip link set %s down 2>/dev/null", interface);
    system(cmd);  // First bring it down to reset state

    snprintf(cmd, sizeof(cmd), "ip link set %s type can bitrate 500000 2>/dev/null", interface);
    int ret = system(cmd);
    if (ret != 0) {
        printf("[SocketCAN] WARNING: Failed to configure %s (may need root privileges)\n", interface);
    }

    snprintf(cmd, sizeof(cmd), "ip link set %s up 2>/dev/null", interface);
    ret = system(cmd);
    if (ret != 0) {
        printf("[SocketCAN] WARNING: Failed to bring up %s (may need root privileges)\n", interface);
    } else {
        printf("[SocketCAN] Interface %s is now UP\n", interface);
    }

    // Create socket
    data->socket_fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (data->socket_fd < 0) {
        perror("[SocketCAN] Failed to create socket");
        delete data;
        return nullptr;
    }

    // Set non-blocking mode
    int flags = fcntl(data->socket_fd, F_GETFL, 0);
    fcntl(data->socket_fd, F_SETFL, flags | O_NONBLOCK);

    // Get interface index
    struct ifreq ifr;
    strncpy(ifr.ifr_name, interface, IFNAMSIZ - 1);
    if (ioctl(data->socket_fd, SIOCGIFINDEX, &ifr) < 0) {
        perror("[SocketCAN] Failed to get interface index");
        close(data->socket_fd);
        delete data;
        return nullptr;
    }

    // Bind socket to CAN interface
    struct sockaddr_can addr;
    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(data->socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[SocketCAN] Failed to bind socket");
        close(data->socket_fd);
        delete data;
        return nullptr;
    }

    printf("[SocketCAN] Initialized on interface: %s\n", interface);
    return data;
}

bool socketcan_receive(SocketCANData* data, CANMessage* msg) {
    if (!data || !msg) return false;

    struct can_frame frame;
    ssize_t nbytes = read(data->socket_fd, &frame, sizeof(struct can_frame));

    if (nbytes < 0) {
        return false;  // No data available (non-blocking)
    }

    if (nbytes < sizeof(struct can_frame)) {
        return false;  // Incomplete frame
    }

    // Copy to CANMessage structure
    msg->can_id = frame.can_id & CAN_EFF_MASK;
    msg->len = frame.can_dlc;
    memcpy(msg->data, frame.data, msg->len);

    return true;
}

bool socketcan_send(SocketCANData* data, const CANMessage* msg) {
    if (!data || !msg) return false;

    struct can_frame frame;
    memset(&frame, 0, sizeof(frame));

    frame.can_id = msg->can_id;
    frame.can_dlc = msg->len;
    memcpy(frame.data, msg->data, msg->len);

    ssize_t nbytes = write(data->socket_fd, &frame, sizeof(struct can_frame));
    return nbytes == sizeof(struct can_frame);
}

void socketcan_cleanup(SocketCANData* data) {
    if (data) {
        if (data->socket_fd >= 0) {
            close(data->socket_fd);
        }
        delete data;
        printf("[SocketCAN] Cleaned up\n");
    }
}
