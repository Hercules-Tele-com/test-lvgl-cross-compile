#include "shared/can_receiver.h"
#include <stdio.h>
#include <string.h>

#ifdef PLATFORM_WINDOWS
  #include "windows/mock_can.h"
#elif defined(PLATFORM_LINUX)
  #include "platform/linux/socketcan.h"
#endif

CANReceiver::CANReceiver() = default;

CANReceiver::~CANReceiver() {
#ifdef PLATFORM_WINDOWS
    if (platform_data) {
        mock_can_cleanup((MockCANData*)platform_data);
    }
#elif defined(PLATFORM_LINUX)
    if (platform_data) {
        struct MultiCan { SocketCANData* ch0; SocketCANData* ch1; };
        auto* mc = (MultiCan*)platform_data;
        if (mc->ch0) socketcan_cleanup(mc->ch0);
        if (mc->ch1) socketcan_cleanup(mc->ch1);
        delete mc;
    }
#endif
}

bool CANReceiver::init() {
#ifdef PLATFORM_WINDOWS
    platform_data = mock_can_init();
    return platform_data != nullptr;

#elif defined(PLATFORM_LINUX)
    struct MultiCan { SocketCANData* ch0; SocketCANData* ch1; };
    auto* mc = new MultiCan();
    mc->ch0 = socketcan_init("can0");
    if (mc->ch0) printf("[CANReceiver] Opened can0\n");
    else         printf("[CANReceiver] WARNING: can0 not available\n");

    mc->ch1 = socketcan_init("can1");
    if (mc->ch1) printf("[CANReceiver] Opened can1\n");
    else         printf("[CANReceiver] INFO: can1 not available (continuing)\n");

    if (!mc->ch0 && !mc->ch1) {
        printf("[CANReceiver] ERROR: no CAN channels opened\n");
        delete mc;
        return false;
    }
    platform_data = mc;
    return true;
#else
    return false;
#endif
}

void CANReceiver::update() {
#ifdef PLATFORM_WINDOWS
    if (platform_data) {
        mock_can_update((MockCANData*)platform_data, this);
    }
#elif defined(PLATFORM_LINUX)
    if (!platform_data) return;
    struct MultiCan { SocketCANData* ch0; SocketCANData* ch1; };
    auto* mc = (MultiCan*)platform_data;

    auto pump = [&](SocketCANData* ch) {
        if (!ch) return;
        CANMessage msg{};
        while (socketcan_receive(ch, &msg)) {
            printf("[CAN %s] ID=%03X DLC=%u DATA=", ch->interface,
                   (msg.can_id & 0x1FFFFFFF), msg.len);
            for (uint8_t i = 0; i < msg.len; ++i) {
                printf("%s%02X", (i ? " " : ""), msg.data[i]);
            }
            printf("\n");
        }
    };

    pump(mc->ch0);
    pump(mc->ch1);
#endif
}

