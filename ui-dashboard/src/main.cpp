#include <iostream>
#include <thread>
#include <chrono>
#include <ctime>

#include "lvgl.h"
#include "can_receiver.h"

// Platform display backends
#ifdef PLATFORM_WINDOWS
#include "windows/sdl_display.h"
#elif defined(PLATFORM_LINUX)
#include "linux/fbdev_display.h"
#endif

// SquareLine UI (C API)
extern "C" {
    #include "ui.h"
}

using namespace std::chrono;

int main(int argc, char* argv[]) {
    std::cout << "=== Nissan Leaf CAN Dashboard ===" << std::endl;

#ifdef PLATFORM_WINDOWS
    std::cout << "Platform: Windows (SDL2 Simulator)" << std::endl;
    if (!sdl_display_init()) {
        std::cerr << "Failed to initialize SDL display" << std::endl;
        return 1;
    }
#elif defined(PLATFORM_LINUX)
    std::cout << "Platform: Linux (Framebuffer)" << std::endl;
    if (!fbdev_display_init()) {
        std::cerr << "Failed to initialize framebuffer display" << std::endl;
        return 1;
    }
#else
    std::cerr << "Unsupported platform" << std::endl;
    return 1;
#endif
    std::cout << "Display initialized" << std::endl;

    // Initialize CAN receiver (mock on Windows, SocketCAN on Linux)
    CANReceiver can;
    if (!can.init()) {
        std::cerr << "Failed to initialize CAN receiver" << std::endl;
#ifdef PLATFORM_WINDOWS
        sdl_display_cleanup();
#elif defined(PLATFORM_LINUX)
        fbdev_display_cleanup();
#endif
        return 1;
    }
    std::cout << "CAN receiver initialized" << std::endl;

    // ----------------------------------------------------------------
    // Load the NEW SquareLine UI (this replaces the old hand-made UI)
    // ----------------------------------------------------------------
    ui_init();  // SquareLine generates and loads the default screen internally

    // If you want to force a specific screen after ui_init(), uncomment and
    // adjust the symbol (e.g., ui_Screen1) to your exported screen name:
    //
    // extern lv_obj_t* ui_Screen1;
    // lv_scr_load(ui_Screen1);

    std::cout << "SquareLine UI initialized" << std::endl;

    // ----------------------------------------------------------------
    // Main loop
    // ----------------------------------------------------------------
    std::cout << "==========================> Entering main loop..." << std::endl;

    while (true) {
        // Time (still available if you later bind it to UI)
        auto time_now = system_clock::now();
        time_t time_t_now = system_clock::to_time_t(time_now);
        struct tm* local_time = localtime(&time_t_now);

        // Update CAN data (mock on Windows will print to console)
        can.update();

        // If you later wire SquareLine widgets, you can push values here.
        // For now we just keep updating LVGL.

#ifdef PLATFORM_WINDOWS
        sdl_display_update();
#elif defined(PLATFORM_LINUX)
        fbdev_display_update();
#endif

        // ~30 FPS
        std::this_thread::sleep_for(milliseconds(33));
    }

    // Cleanup (unreached with current loop, but kept for completeness)
#ifdef PLATFORM_WINDOWS
    sdl_display_cleanup();
#elif defined(PLATFORM_LINUX)
    fbdev_display_cleanup();
#endif

    return 0;
}
