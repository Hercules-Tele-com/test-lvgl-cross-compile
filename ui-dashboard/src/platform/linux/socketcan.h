#ifndef SOCKETCAN_H
#define SOCKETCAN_H

#include <stdint.h>

struct CANMessage {
    uint32_t can_id;
    uint8_t data[8];
    uint8_t len;
};

struct SocketCANData {
    int socket_fd;
    char interface[16];
};

// Initialize SocketCAN
SocketCANData* socketcan_init(const char* interface);

// Receive CAN message (non-blocking)
bool socketcan_receive(SocketCANData* data, CANMessage* msg);

// Send CAN message
bool socketcan_send(SocketCANData* data, const CANMessage* msg);

// Cleanup
void socketcan_cleanup(SocketCANData* data);

#endif // SOCKETCAN_H
