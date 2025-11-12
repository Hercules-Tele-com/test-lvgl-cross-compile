#ifndef MOCK_CAN_H
#define MOCK_CAN_H

#include <stdint.h>

// Forward declaration
class CANReceiver;

enum class CANSourceMode {
    PCAN_HARDWARE,  // CANable Pro via PCAN driver
    DEMO_PLAYBACK   // Playback from can_log_demo.txt
};

struct MockCANData {
    uint32_t last_update_ms;
    uint32_t update_counter;
    void* internal_data;  // MockCANDataInternal*
    CANSourceMode mode;
};

// Initialize CAN (tries PCAN hardware first, falls back to demo playback)
MockCANData* mock_can_init();

// Update CAN (reads from PCAN or plays back demo data)
void mock_can_update(MockCANData* data, CANReceiver* receiver);

// Cleanup
void mock_can_cleanup(MockCANData* data);

#endif // MOCK_CAN_H
