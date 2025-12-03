#include "mock_can.h"
#include "can_receiver.h"
#include <stdlib.h>
#include <string.h>
#include <chrono>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

// Try to include PCAN-Basic header if available
// Download from: https://www.peak-system.com/PCAN-Basic.239.0.html
#ifdef _WIN32
    // Try to dynamically load PCAN-Basic.dll if available
    #include <windows.h>

    // PCAN-Basic definitions (minimal subset needed)
    #define PCAN_USBBUS1      0x51U
    #define PCAN_BAUD_500K    0x001CU
    #define PCAN_ERROR_OK     0x00000U
    #define PCAN_ERROR_QRCVEMPTY 0x00032U

    #define TPCANHandle       uint16_t
    #define TPCANStatus       uint32_t
    #define TPCANBaudrate     uint16_t

    typedef struct tagTPCANMsg {
        uint32_t ID;
        uint8_t  MSGTYPE;
        uint8_t  LEN;
        uint8_t  DATA[8];
    } TPCANMsg;

    // Function pointers for PCAN-Basic DLL
    typedef TPCANStatus (__stdcall *PCAN_Initialize_t)(TPCANHandle, TPCANBaudrate, uint32_t, uint32_t, uint32_t);
    typedef TPCANStatus (__stdcall *PCAN_Uninitialize_t)(TPCANHandle);
    typedef TPCANStatus (__stdcall *PCAN_Read_t)(TPCANHandle, TPCANMsg*, void*);

    static HMODULE g_pcan_dll = nullptr;
    static PCAN_Initialize_t g_pcan_initialize = nullptr;
    static PCAN_Uninitialize_t g_pcan_uninitialize = nullptr;
    static PCAN_Read_t g_pcan_read = nullptr;
    static TPCANHandle g_pcan_handle = 0;

    static bool load_pcan_dll() {
        g_pcan_dll = LoadLibraryA("PCANBasic.dll");
        if (!g_pcan_dll) {
            std::cout << "[PCAN] PCANBasic.dll not found (install PEAK drivers from peak-system.com)" << std::endl;
            return false;
        }

        g_pcan_initialize = (PCAN_Initialize_t)GetProcAddress(g_pcan_dll, "CAN_Initialize");
        g_pcan_uninitialize = (PCAN_Uninitialize_t)GetProcAddress(g_pcan_dll, "CAN_Uninitialize");
        g_pcan_read = (PCAN_Read_t)GetProcAddress(g_pcan_dll, "CAN_Read");

        if (!g_pcan_initialize || !g_pcan_uninitialize || !g_pcan_read) {
            std::cerr << "[PCAN] Failed to load PCAN functions from DLL" << std::endl;
            FreeLibrary(g_pcan_dll);
            g_pcan_dll = nullptr;
            return false;
        }

        return true;
    }
#endif

using namespace std::chrono;

struct CANLogEntry {
    double timestamp;
    uint32_t can_id;
    uint8_t dlc;
    uint8_t data[8];
};

struct MockCANDataInternal {
    // Demo playback mode
    std::vector<CANLogEntry> log_entries;
    size_t current_index;
    steady_clock::time_point start_time;
    double playback_start_timestamp;
    bool loop_enabled;
    uint32_t loop_count;

    // PCAN mode
    bool pcan_active;
    uint32_t pcan_msg_count;
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

static bool load_demo_log_file(MockCANDataInternal* internal) {
    // Try multiple possible locations for the log file
    std::vector<std::string> possible_paths = {
        // Direct path in source tree
        "src/platform/windows/can_log_demo.txt",
        "../src/platform/windows/can_log_demo.txt",
        "../../src/platform/windows/can_log_demo.txt",
        "../../../src/platform/windows/can_log_demo.txt",

        // Relative to build directory (Debug/Release)
        "../../src/platform/windows/can_log_demo.txt",
        "../../../src/platform/windows/can_log_demo.txt",

        // Current directory and parent directories
        "can_log_demo.txt",
        "../can_log_demo.txt",
        "../../can_log_demo.txt",
        "../../../can_log_demo.txt",
        "../../../../can_log_demo.txt",
        "../../../../../can_log_demo.txt",

        // Absolute paths (user-specific)
        "C:\\Users\\Mike\\Repositories\\test-lvgl-cross-compile\\ui-dashboard\\src\\platform\\windows\\can_log_demo.txt",
        "C:\\Users\\Mike\\Repositories\\leaf_cruiser\\can_log_demo.txt",
        "C:\\Users\\Mike\\Repositories\\leaf_cruiser\\test-lvgl-cross-compile\\ui-dashboard\\src\\platform\\windows\\can_log_demo.txt",
    };

    std::ifstream log_file;
    std::string used_path;

    std::cout << "[DemoPlayback] Searching for can_log_demo.txt..." << std::endl;

    for (const auto& path : possible_paths) {
        log_file.open(path);
        if (log_file.is_open()) {
            used_path = path;
            break;
        }
    }

    if (!log_file.is_open()) {
        std::cerr << "\n[DemoPlayback] ============================================" << std::endl;
        std::cerr << "[DemoPlayback] ERROR: Could not find can_log_demo.txt" << std::endl;
        std::cerr << "[DemoPlayback] ============================================" << std::endl;
        std::cerr << "[DemoPlayback] Searched in the following locations:" << std::endl;
        for (const auto& path : possible_paths) {
            std::cerr << "  [✗] " << path << std::endl;
        }
        std::cerr << "\n[DemoPlayback] SOLUTION:" << std::endl;
        std::cerr << "  1. Copy can_log_demo.txt from:" << std::endl;
        std::cerr << "     ui-dashboard/src/platform/windows/can_log_demo.txt" << std::endl;
        std::cerr << "  2. To the same directory as the executable" << std::endl;
        std::cerr << "  3. Or run the executable from the build directory" << std::endl;
        std::cerr << "[DemoPlayback] ============================================\n" << std::endl;
        return false;
    }

    std::cout << "[DemoPlayback] ✓ Found CAN log file: " << used_path << std::endl;

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
        std::cerr << "[DemoPlayback] ERROR: No valid CAN messages found in log file" << std::endl;
        std::cerr << "[DemoPlayback] File format should be: Timestamp CAN_ID DLC Data_Bytes" << std::endl;
        std::cerr << "[DemoPlayback] Example: 0.000000 351 8 0B B8 00 00 00 00 00 00" << std::endl;
        return false;
    }

    // Set the playback start timestamp to the first entry's timestamp
    internal->playback_start_timestamp = internal->log_entries[0].timestamp;

    std::cout << "[DemoPlayback] ✓ Loaded " << parsed_count << " CAN messages from "
              << line_num << " lines" << std::endl;
    std::cout << "[DemoPlayback] Time range: " << internal->log_entries[0].timestamp
              << "s to " << internal->log_entries.back().timestamp << "s ("
              << (internal->log_entries.back().timestamp - internal->log_entries[0].timestamp)
              << "s duration)" << std::endl;
    std::cout << "[DemoPlayback] Playback will loop continuously" << std::endl;

    return true;
}

MockCANData* mock_can_init() {
    std::cout << "\n=== CAN Initialization ===" << std::endl;

    auto* internal = new MockCANDataInternal();
    internal->current_index = 0;
    internal->start_time = steady_clock::now();
    internal->playback_start_timestamp = 0.0;
    internal->loop_enabled = true;
    internal->loop_count = 0;
    internal->pcan_active = false;
    internal->pcan_msg_count = 0;

    MockCANData* data = new MockCANData();
    data->last_update_ms = get_millis();
    data->update_counter = 0;
    data->internal_data = internal;

#ifdef _WIN32
    // Step 1: Try to load PCAN-Basic DLL
    std::cout << "[CAN] Attempting to connect to CANable Pro via PCAN..." << std::endl;

    if (load_pcan_dll()) {
        // Step 2: Try to initialize PCAN_USBBUS1 at 500kbps
        g_pcan_handle = PCAN_USBBUS1;
        TPCANStatus status = g_pcan_initialize(g_pcan_handle, PCAN_BAUD_500K, 0, 0, 0);

        if (status == PCAN_ERROR_OK) {
            std::cout << "[PCAN] ✓ Successfully connected to CANable Pro (PCAN_USBBUS1 @ 500kbps)" << std::endl;
            std::cout << "[PCAN] Reading live CAN messages..." << std::endl;
            internal->pcan_active = true;
            data->mode = CANSourceMode::PCAN_HARDWARE;
            return data;
        } else {
            std::cout << "[PCAN] × Failed to initialize PCAN_USBBUS1 (error code: 0x"
                      << std::hex << status << std::dec << ")" << std::endl;
            std::cout << "[PCAN] CANable Pro not detected or in use by another application" << std::endl;

            // Clean up DLL
            if (g_pcan_dll) {
                FreeLibrary(g_pcan_dll);
                g_pcan_dll = nullptr;
            }
        }
    }
#endif

    // Step 3: Fall back to demo playback
    std::cout << "[CAN] Falling back to demo playback mode..." << std::endl;

    if (!load_demo_log_file(internal)) {
        std::cerr << "[CAN] ERROR: Failed to initialize any CAN source!" << std::endl;
        delete internal;
        delete data;
        return nullptr;
    }

    data->mode = CANSourceMode::DEMO_PLAYBACK;
    std::cout << "=========================\n" << std::endl;
    return data;
}

void mock_can_update(MockCANData* data, CANReceiver* receiver) {
    if (!data || !data->internal_data) return;

    auto* internal = static_cast<MockCANDataInternal*>(data->internal_data);

    // PCAN hardware mode: read live messages
    if (data->mode == CANSourceMode::PCAN_HARDWARE && internal->pcan_active) {
#ifdef _WIN32
        if (g_pcan_read) {
            // Read up to 100 messages per update to avoid blocking
            for (int i = 0; i < 100; ++i) {
                TPCANMsg msg;
                TPCANStatus status = g_pcan_read(g_pcan_handle, &msg, nullptr);

                if (status == PCAN_ERROR_OK) {
                    // Successfully read a message
                    receiver->processCANMessage(msg.ID, msg.LEN, msg.DATA);
                    internal->pcan_msg_count++;
                    data->update_counter++;

                    // Print every 100th message as a heartbeat
                    if (internal->pcan_msg_count % 100 == 0) {
                        std::cout << "[PCAN] Received " << internal->pcan_msg_count << " messages" << std::endl;
                    }
                } else if (status == PCAN_ERROR_QRCVEMPTY) {
                    // No more messages in queue
                    break;
                } else {
                    // Other error - print once and continue
                    static bool error_printed = false;
                    if (!error_printed) {
                        std::cerr << "[PCAN] Read error: 0x" << std::hex << status << std::dec << std::endl;
                        error_printed = true;
                    }
                    break;
                }
            }
        }
#endif
        return;
    }

    // Demo playback mode
    if (data->mode == CANSourceMode::DEMO_PLAYBACK) {
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
            std::cout << "[DemoPlayback] Looping playback (loop #" << internal->loop_count << ")" << std::endl;
        }
    }
}

void mock_can_cleanup(MockCANData* data) {
    if (data) {
        auto* internal = static_cast<MockCANDataInternal*>(data->internal_data);

#ifdef _WIN32
        // Clean up PCAN if active
        if (data->mode == CANSourceMode::PCAN_HARDWARE && internal && internal->pcan_active) {
            if (g_pcan_uninitialize) {
                g_pcan_uninitialize(g_pcan_handle);
                std::cout << "[PCAN] Disconnected from CANable Pro" << std::endl;
            }
            if (g_pcan_dll) {
                FreeLibrary(g_pcan_dll);
                g_pcan_dll = nullptr;
            }
        }
#endif

        if (internal) {
            if (data->mode == CANSourceMode::DEMO_PLAYBACK) {
                std::cout << "[DemoPlayback] Cleaned up (processed " << data->update_counter << " messages)" << std::endl;
            }
            delete internal;
        }
        delete data;
    }
}
