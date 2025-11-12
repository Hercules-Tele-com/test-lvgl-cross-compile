#include "mock_can.h"
#include "can_receiver.h"
#include <stdlib.h>
#include <string.h>
#include <chrono>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

using namespace std::chrono;

struct CANLogEntry {
    double timestamp;
    uint32_t can_id;
    uint8_t dlc;
    uint8_t data[8];
};

struct MockCANDataInternal {
    std::vector<CANLogEntry> log_entries;
    size_t current_index;
    steady_clock::time_point start_time;
    double playback_start_timestamp;
    bool loop_enabled;
    uint32_t loop_count;
};

static uint32_t get_millis() {
    static auto start = steady_clock::now();
    auto now = steady_clock::now();
    return duration_cast<milliseconds>(now - start).count();
}

static bool parse_can_log_line(const std::string& line, CANLogEntry& entry) {
    // Format: Timestamp(s) CAN_ID DLC Data
    // Example: 0.000000	351	8	0B B8 00 00 00 00 00 00

    std::istringstream iss(line);
    std::string timestamp_str, can_id_str, dlc_str;

    // Skip comment lines and header
    if (line.empty() || line[0] == '#') {
        return false;
    }

    // Parse timestamp
    if (!(iss >> timestamp_str)) return false;
    entry.timestamp = std::stod(timestamp_str);

    // Parse CAN ID (hex)
    if (!(iss >> std::hex >> entry.can_id)) return false;

    // Parse DLC
    if (!(iss >> std::dec >> dlc_str)) return false;
    entry.dlc = std::stoul(dlc_str);

    if (entry.dlc > 8) return false;

    // Parse data bytes (hex)
    for (uint8_t i = 0; i < entry.dlc; ++i) {
        uint32_t byte_val;
        if (!(iss >> std::hex >> byte_val)) return false;
        entry.data[i] = (uint8_t)byte_val;
    }

    return true;
}

MockCANData* mock_can_init() {
    auto* internal = new MockCANDataInternal();
    internal->current_index = 0;
    internal->start_time = steady_clock::now();
    internal->playback_start_timestamp = 0.0;
    internal->loop_enabled = true;
    internal->loop_count = 0;

    // Try multiple possible locations for the log file
    std::vector<std::string> possible_paths = {
        "C:\\Users\\Mike\\Repositories\\leaf_cruiser\\can_log_demo.txt",
        "C:\\Users\\Mike\\Repositories\\leaf_cruiser\\test-lvgl-cross-compile\\ui-dashboard\\src\\platform\\windows\\can_log_demo.txt",
        "can_log_demo.txt",
        "../can_log_demo.txt",
        "../../can_log_demo.txt",
        "../../../can_log_demo.txt",
        "../../../../can_log_demo.txt",
        "../../../../../can_log_demo.txt"
    };

    std::ifstream log_file;
    std::string used_path;

    for (const auto& path : possible_paths) {
        log_file.open(path);
        if (log_file.is_open()) {
            used_path = path;
            break;
        }
    }

    if (!log_file.is_open()) {
        std::cerr << "[MockCAN] ERROR: Could not open can_log_demo.txt from any location" << std::endl;
        std::cerr << "[MockCAN] Tried paths:" << std::endl;
        for (const auto& path : possible_paths) {
            std::cerr << "  - " << path << std::endl;
        }
        delete internal;
        return nullptr;
    }

    std::cout << "[MockCAN] Loading CAN log from: " << used_path << std::endl;

    // Read all log entries
    std::string line;
    int line_num = 0;
    int parsed_count = 0;

    while (std::getline(log_file, line)) {
        line_num++;
        CANLogEntry entry;
        if (parse_can_log_line(line, entry)) {
            internal->log_entries.push_back(entry);
            parsed_count++;
        }
    }

    log_file.close();

    if (internal->log_entries.empty()) {
        std::cerr << "[MockCAN] ERROR: No valid CAN messages found in log file" << std::endl;
        delete internal;
        return nullptr;
    }

    // Set the playback start timestamp to the first entry's timestamp
    internal->playback_start_timestamp = internal->log_entries[0].timestamp;

    std::cout << "[MockCAN] Loaded " << parsed_count << " CAN messages from "
              << line_num << " lines" << std::endl;
    std::cout << "[MockCAN] Time range: " << internal->log_entries[0].timestamp
              << "s to " << internal->log_entries.back().timestamp << "s" << std::endl;
    std::cout << "[MockCAN] Playback will loop continuously" << std::endl;

    MockCANData* data = new MockCANData();
    data->last_update_ms = get_millis();
    data->update_counter = 0;
    data->internal_data = internal;

    return data;
}

void mock_can_update(MockCANData* data, CANReceiver* receiver) {
    if (!data || !data->internal_data) return;

    auto* internal = static_cast<MockCANDataInternal*>(data->internal_data);

    if (internal->log_entries.empty()) return;

    // Calculate elapsed time since start
    auto now = steady_clock::now();
    double elapsed_sec = duration_cast<milliseconds>(now - internal->start_time).count() / 1000.0;

    // Adjust for the log's start timestamp
    double playback_time = elapsed_sec + internal->playback_start_timestamp;

    // Process all messages that should have been sent by now
    while (internal->current_index < internal->log_entries.size()) {
        const auto& entry = internal->log_entries[internal->current_index];

        if (entry.timestamp > playback_time) {
            // This message is in the future, wait for next update
            break;
        }

        // Send this message to the receiver
        receiver->processCANMessage(entry.can_id, entry.dlc, entry.data);

        internal->current_index++;
        data->update_counter++;
    }

    // Loop back to the beginning if we've processed all messages
    if (internal->current_index >= internal->log_entries.size() && internal->loop_enabled) {
        internal->current_index = 0;
        internal->start_time = now;
        internal->playback_start_timestamp = internal->log_entries[0].timestamp;
        internal->loop_count++;
        std::cout << "[MockCAN] Looping playback (loop #" << internal->loop_count << ")" << std::endl;
    }
}

void mock_can_cleanup(MockCANData* data) {
    if (data) {
        if (data->internal_data) {
            delete static_cast<MockCANDataInternal*>(data->internal_data);
        }
        delete data;
        std::cout << "[MockCAN] Cleaned up" << std::endl;
    }
}
