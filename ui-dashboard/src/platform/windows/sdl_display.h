#ifndef SDL_DISPLAY_H
#define SDL_DISPLAY_H

#include "lvgl.h"

// Initialize SDL2 display for LVGL
bool sdl_display_init();

// Update SDL2 display
void sdl_display_update();

// Cleanup SDL2 display
void sdl_display_cleanup();

#endif // SDL_DISPLAY_H
