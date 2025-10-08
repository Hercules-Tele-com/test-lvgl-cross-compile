#ifndef FBDEV_DISPLAY_H
#define FBDEV_DISPLAY_H

#include "lvgl.h"

// Initialize framebuffer display for LVGL
bool fbdev_display_init();

// Update framebuffer display
void fbdev_display_update();

// Cleanup framebuffer display
void fbdev_display_cleanup();

#endif // FBDEV_DISPLAY_H
