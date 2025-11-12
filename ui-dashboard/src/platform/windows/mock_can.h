#ifndef MOCK_CAN_H
#define MOCK_CAN_H

#include <stdint.h>

// Forward declaration
class CANReceiver;

struct MockCANData {
    uint32_t last_update_ms;
    uint32_t update_counter;
    void* internal_data;  // MockCANDataInternal*
};

// Initialize mock CAN
MockCANData* mock_can_init();

// Update mock CAN (generates fake data)
void mock_can_update(MockCANData* data, CANReceiver* receiver);

// Cleanup
void mock_can_cleanup(MockCANData* data);

#endif // MOCK_CAN_H
