#ifndef LEAF_CAN_BUS_H
#define LEAF_CAN_BUS_H

#include <Arduino.h>
#include "driver/twai.h"
#include "LeafCANMessages.h"

// Maximum number of subscriptions and publishers
#define MAX_SUBSCRIPTIONS 16
#define MAX_PUBLISHERS 8

// CAN bus configuration
#define CAN_SPEED_500KBPS TWAI_TIMING_CONFIG_500KBITS()
#define CAN_TX_GPIO_NUM GPIO_NUM_5
#define CAN_RX_GPIO_NUM GPIO_NUM_4

// Subscription callback type
typedef void (*can_unpack_callback_t)(const uint8_t* data, uint8_t len, void* state);

// Publisher pack callback type
typedef void (*can_pack_callback_t)(const void* state, uint8_t* data, uint8_t* len);

// Subscription entry
typedef struct {
    uint32_t can_id;
    can_unpack_callback_t unpack_fn;
    void* state_ptr;
    bool active;
} Subscription;

// Publisher entry
typedef struct {
    uint32_t can_id;
    can_pack_callback_t pack_fn;
    void* state_ptr;
    uint32_t interval_ms;
    uint32_t last_publish_ms;
    bool active;
} Publisher;

class LeafCANBus {
public:
    LeafCANBus();
    ~LeafCANBus();

    // Initialize CAN bus with custom pins (optional)
    bool begin(gpio_num_t tx_pin = CAN_TX_GPIO_NUM, gpio_num_t rx_pin = CAN_RX_GPIO_NUM);

    // Subscribe to a CAN ID
    bool subscribe(uint32_t can_id, can_unpack_callback_t unpack_fn, void* state_ptr);

    // Publish a CAN message periodically
    bool publish(uint32_t can_id, uint32_t interval_ms, can_pack_callback_t pack_fn, void* state_ptr);

    // Send a CAN message immediately (ad-hoc)
    bool send(uint32_t can_id, const uint8_t* data, uint8_t len);

    // Process CAN bus (call in loop())
    void process();

    // Stop CAN bus
    void end();

    // Get statistics
    uint32_t getRxCount() const { return rx_count; }
    uint32_t getTxCount() const { return tx_count; }
    uint32_t getErrorCount() const { return error_count; }

private:
    // RX task handle
    static TaskHandle_t rx_task_handle;
    static QueueHandle_t rx_queue;

    // RX task function
    static void rx_task(void* pvParameters);

    // Process received message
    void processRxMessage(const twai_message_t& message);

    // Process publishers (periodic sending)
    void processPublishers();

    // Subscriptions and publishers
    Subscription subscriptions[MAX_SUBSCRIPTIONS];
    Publisher publishers[MAX_PUBLISHERS];

    // Statistics
    uint32_t rx_count;
    uint32_t tx_count;
    uint32_t error_count;

    // Initialization flag
    bool initialized;
};

#endif // LEAF_CAN_BUS_H
