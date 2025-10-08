#include "sdl_display.h"
#include <SDL2/SDL.h>
#include <iostream>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 480

static SDL_Window* window = nullptr;
static SDL_Renderer* renderer = nullptr;
static lv_disp_drv_t disp_drv;
static lv_disp_draw_buf_t disp_buf;
static lv_color_t* buf1 = nullptr;
static lv_color_t* buf2 = nullptr;

// Flush callback for LVGL
static void sdl_display_flush(lv_disp_drv_t* disp_drv, const lv_area_t* area, lv_color_t* color_p) {
    if (!renderer) return;

    int32_t x, y;
    for (y = area->y1; y <= area->y2; y++) {
        for (x = area->x1; x <= area->x2; x++) {
            lv_color_t c = *color_p;
            SDL_SetRenderDrawColor(renderer, c.ch.red, c.ch.green, c.ch.blue, 0xFF);
            SDL_RenderDrawPoint(renderer, x, y);
            color_p++;
        }
    }

    lv_disp_flush_ready(disp_drv);
}

bool sdl_display_init() {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "[SDL] Failed to initialize: " << SDL_GetError() << std::endl;
        return false;
    }

    // Create window
    window = SDL_CreateWindow(
        "Leaf CAN Dashboard",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN
    );

    if (!window) {
        std::cerr << "[SDL] Failed to create window: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return false;
    }

    // Create renderer
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        std::cerr << "[SDL] Failed to create renderer: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }

    // Initialize LVGL
    lv_init();

    // Allocate draw buffers
    buf1 = new lv_color_t[WINDOW_WIDTH * 100];
    buf2 = new lv_color_t[WINDOW_WIDTH * 100];

    // Initialize display buffer
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, WINDOW_WIDTH * 100);

    // Initialize display driver
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = sdl_display_flush;
    disp_drv.hor_res = WINDOW_WIDTH;
    disp_drv.ver_res = WINDOW_HEIGHT;
    lv_disp_drv_register(&disp_drv);

    std::cout << "[SDL] Display initialized (" << WINDOW_WIDTH << "x" << WINDOW_HEIGHT << ")" << std::endl;
    return true;
}

void sdl_display_update() {
    if (!renderer) return;

    // Handle SDL events
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            exit(0);
        }
    }

    // Clear screen
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);
    SDL_RenderClear(renderer);

    // LVGL timer handler
    lv_timer_handler();

    // Present renderer
    SDL_RenderPresent(renderer);
}

void sdl_display_cleanup() {
    if (buf1) delete[] buf1;
    if (buf2) delete[] buf2;
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
    std::cout << "[SDL] Display cleaned up" << std::endl;
}
