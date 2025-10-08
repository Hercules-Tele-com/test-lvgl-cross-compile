#include <iostream>
#include <thread>
#include <chrono>
#include "lvgl.h"
#include "dashboard_ui.h"
#include "can_receiver.h"

#ifdef PLATFORM_WINDOWS
#include "windows/sdl_display.h"
#elif defined(PLATFORM_LINUX)
#include "linux/fbdev_display.h"
#endif

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

    // Initialize CAN receiver
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

    // Initialize dashboard UI
    DashboardUI dashboard;
    dashboard.init();
    std::cout << "Dashboard UI initialized" << std::endl;

    // Main loop
    std::cout << "Entering main loop..." << std::endl;
    auto last_update = steady_clock::now();

    while (true) {
        auto now = steady_clock::now();
        auto elapsed = duration_cast<milliseconds>(now - last_update).count();

        // Update CAN data
        can.update();

        // Update UI with CAN data
        dashboard.update(can);

        // Update display
#ifdef PLATFORM_WINDOWS
        sdl_display_update();
#elif defined(PLATFORM_LINUX)
        fbdev_display_update();
#endif

        // Sleep to maintain ~30 FPS
        std::this_thread::sleep_for(milliseconds(33));

        last_update = now;
    }

    // Cleanup (unreachable in current loop, but good practice)
#ifdef PLATFORM_WINDOWS
    sdl_display_cleanup();
#elif defined(PLATFORM_LINUX)
    fbdev_display_cleanup();
#endif

    return 0;
}
