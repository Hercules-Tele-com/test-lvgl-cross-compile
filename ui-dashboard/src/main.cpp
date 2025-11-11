#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include <ctime>

#include "lvgl.h"
#include "can_receiver.h"

#ifdef PLATFORM_WINDOWS
  #include "windows/sdl_display.h"
#elif defined(PLATFORM_LINUX)
  #include "linux/fbdev_display.h"
#endif

extern "C" {
  #include "ui.h"
}

// -------- LVGL tick thread --------
static std::atomic<bool> g_running{true};
static void sig_handler(int) { g_running = false; }

static void lvgl_tick_thread() {
  using namespace std::chrono;
  while (g_running) {
    std::this_thread::sleep_for(milliseconds(1));
    lv_tick_inc(1);
  }
}

int main(int, char**) {
  std::cout << "=== Nissan Leaf CAN Dashboard ===\n";

#ifdef PLATFORM_WINDOWS
  std::cout << "Platform: Windows (SDL2 Simulator)\n";
  if (!sdl_display_init()) { std::cerr << "Failed to init SDL display\n"; return 1; }
#elif defined(PLATFORM_LINUX)
  std::cout << "Platform: Linux (Framebuffer)\n";
  if (!fbdev_display_init()) { std::cerr << "Failed to init framebuffer display\n"; return 1; }
#else
  std::cerr << "Unsupported platform\n";
  return 1;
#endif
  std::cout << "Display initialized\n";

  // CAN
  CANReceiver can;
  if (!can.init()) {
#ifdef PLATFORM_WINDOWS
    sdl_display_cleanup();
#elif defined(PLATFORM_LINUX)
    fbdev_display_cleanup();
#endif
    std::cerr << "Failed to initialize CAN receiver\n";
    return 1;
  }
  std::cout << "CAN receiver initialized\n";

  // SquareLine UI
  ui_init();
  std::cout << "SquareLine UI initialized\n";

  // Signals + tick
  std::signal(SIGINT,  sig_handler);
  std::signal(SIGTERM, sig_handler);
  std::thread tick_thr(lvgl_tick_thread);

  std::cout << "==========================> Entering main loop...\n";
  while (g_running) {
    // Update data
    can.update();

    // Let LVGL do its work (timers, animations, draw)
    lv_timer_handler();  // <— this is essential on Linux
#ifdef PLATFORM_WINDOWS
    sdl_display_update();
#elif defined(PLATFORM_LINUX)
    fbdev_display_update();
#endif

    // ~30 FPS pacing
    std::this_thread::sleep_for(std::chrono::milliseconds(33));
  }

  if (tick_thr.joinable()) tick_thr.join();

#ifdef PLATFORM_WINDOWS
  sdl_display_cleanup();
#elif defined(PLATFORM_LINUX)
  fbdev_display_cleanup();
#endif
  return 0;
}

