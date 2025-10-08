#include "LeafCANBus.h"

// Static member initialization
TaskHandle_t LeafCANBus::rx_task_handle = nullptr;
QueueHandle_t LeafCANBus::rx_queue = nullptr;

LeafCANBus::LeafCANBus() : rx_count(0), tx_count(0), error_count(0), initialized(false) {
    // Initialize subscription array
    for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
        subscriptions[i].active = false;
    }

    // Initialize publisher array
    for (int i = 0; i < MAX_PUBLISHERS; i++) {
        publishers[i].active = false;
    }
}

LeafCANBus::~LeafCANBus() {
    end();
}

bool LeafCANBus::begin(gpio_num_t tx_pin, gpio_num_t rx_pin) {
    if (initialized) {
        Serial.println("[CAN] Already initialized");
        return false;
    }

    // Configure TWAI timing (500 kbps)
    twai_timing_config_t t_config = CAN_SPEED_500KBPS;

    // Configure TWAI filter (accept all messages)
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    // Configure TWAI general settings
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(tx_pin, rx_pin, TWAI_MODE_NORMAL);
    g_config.rx_queue_len = 20;  // Increase RX queue length

    // Install TWAI driver
    esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);
    if (err != ESP_OK) {
        Serial.printf("[CAN] Failed to install driver: %d\n", err);
        return false;
    }

    // Start TWAI driver
    err = twai_start();
    if (err != ESP_OK) {
        Serial.printf("[CAN] Failed to start driver: %d\n", err);
        twai_driver_uninstall();
        return false;
    }

    // Create RX queue
    rx_queue = xQueueCreate(20, sizeof(twai_message_t));
    if (rx_queue == nullptr) {
        Serial.println("[CAN] Failed to create RX queue");
        twai_stop();
        twai_driver_uninstall();
        return false;
    }

    // Create RX task
    BaseType_t task_created = xTaskCreatePinnedToCore(
        rx_task,
        "can_rx_task",
        4096,
        this,
        5,  // Priority
        &rx_task_handle,
        1   // Core 1
    );

    if (task_created != pdPASS) {
        Serial.println("[CAN] Failed to create RX task");
        vQueueDelete(rx_queue);
        rx_queue = nullptr;
        twai_stop();
        twai_driver_uninstall();
        return false;
    }

    initialized = true;
    Serial.printf("[CAN] Initialized on TX=%d, RX=%d\n", tx_pin, rx_pin);
    return true;
}

void LeafCANBus::rx_task(void* pvParameters) {
    LeafCANBus* instance = (LeafCANBus*)pvParameters;
    twai_message_t message;

    while (true) {
        // Wait for message from TWAI driver
        esp_err_t err = twai_receive(&message, pdMS_TO_TICKS(100));
        if (err == ESP_OK) {
            // Add to queue for processing in main loop
            if (xQueueSend(rx_queue, &message, 0) != pdTRUE) {
                instance->error_count++;
            }
        } else if (err == ESP_ERR_TIMEOUT) {
            // No message received, continue
            continue;
        } else {
            // Error receiving message
            instance->error_count++;
        }

        // Check for bus-off state and attempt recovery
        twai_status_info_t status;
        if (twai_get_status_info(&status) == ESP_OK) {
            if (status.state == TWAI_STATE_BUS_OFF) {
                Serial.println("[CAN] Bus-off detected, attempting recovery...");
                twai_initiate_recovery();
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        }
    }
}

bool LeafCANBus::subscribe(uint32_t can_id, can_unpack_callback_t unpack_fn, void* state_ptr) {
    if (!initialized) {
        Serial.println("[CAN] Not initialized");
        return false;
    }

    // Find empty slot
    for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
        if (!subscriptions[i].active) {
            subscriptions[i].can_id = can_id;
            subscriptions[i].unpack_fn = unpack_fn;
            subscriptions[i].state_ptr = state_ptr;
            subscriptions[i].active = true;
            Serial.printf("[CAN] Subscribed to 0x%03X\n", can_id);
            return true;
        }
    }

    Serial.println("[CAN] No subscription slots available");
    return false;
}

bool LeafCANBus::publish(uint32_t can_id, uint32_t interval_ms, can_pack_callback_t pack_fn, void* state_ptr) {
    if (!initialized) {
        Serial.println("[CAN] Not initialized");
        return false;
    }

    // Find empty slot
    for (int i = 0; i < MAX_PUBLISHERS; i++) {
        if (!publishers[i].active) {
            publishers[i].can_id = can_id;
            publishers[i].pack_fn = pack_fn;
            publishers[i].state_ptr = state_ptr;
            publishers[i].interval_ms = interval_ms;
            publishers[i].last_publish_ms = 0;
            publishers[i].active = true;
            Serial.printf("[CAN] Publisher added for 0x%03X (interval: %u ms)\n", can_id, interval_ms);
            return true;
        }
    }

    Serial.println("[CAN] No publisher slots available");
    return false;
}

bool LeafCANBus::send(uint32_t can_id, const uint8_t* data, uint8_t len) {
    if (!initialized) {
        return false;
    }

    if (len > 8) {
        Serial.println("[CAN] Data length exceeds 8 bytes");
        return false;
    }

    twai_message_t message;
    message.identifier = can_id;
    message.data_length_code = len;
    message.flags = TWAI_MSG_FLAG_NONE;
    memcpy(message.data, data, len);

    esp_err_t err = twai_transmit(&message, pdMS_TO_TICKS(10));
    if (err == ESP_OK) {
        tx_count++;
        return true;
    } else {
        error_count++;
        return false;
    }
}

void LeafCANBus::processRxMessage(const twai_message_t& message) {
    rx_count++;

    // Check subscriptions
    for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
        if (subscriptions[i].active && subscriptions[i].can_id == message.identifier) {
            if (subscriptions[i].unpack_fn && subscriptions[i].state_ptr) {
                subscriptions[i].unpack_fn(message.data, message.data_length_code, subscriptions[i].state_ptr);
            }
        }
    }
}

void LeafCANBus::processPublishers() {
    uint32_t now = millis();

    for (int i = 0; i < MAX_PUBLISHERS; i++) {
        if (!publishers[i].active) continue;

        // Check if it's time to publish
        if (now - publishers[i].last_publish_ms >= publishers[i].interval_ms) {
            uint8_t data[8];
            uint8_t len = 0;

            // Pack data
            if (publishers[i].pack_fn && publishers[i].state_ptr) {
                publishers[i].pack_fn(publishers[i].state_ptr, data, &len);

                // Send message
                if (send(publishers[i].can_id, data, len)) {
                    publishers[i].last_publish_ms = now;
                }
            }
        }
    }
}

void LeafCANBus::process() {
    if (!initialized) return;

    // Process RX queue
    twai_message_t message;
    while (xQueueReceive(rx_queue, &message, 0) == pdTRUE) {
        processRxMessage(message);
    }

    // Process publishers
    processPublishers();
}

void LeafCANBus::end() {
    if (!initialized) return;

    // Delete RX task
    if (rx_task_handle != nullptr) {
        vTaskDelete(rx_task_handle);
        rx_task_handle = nullptr;
    }

    // Delete RX queue
    if (rx_queue != nullptr) {
        vQueueDelete(rx_queue);
        rx_queue = nullptr;
    }

    // Stop and uninstall TWAI driver
    twai_stop();
    twai_driver_uninstall();

    initialized = false;
    Serial.println("[CAN] Stopped");
}
