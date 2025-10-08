#include "fbdev_display.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

static int fbfd = -1;
static struct fb_var_screeninfo vinfo;
static struct fb_fix_screeninfo finfo;
static uint8_t* fbp = nullptr;
static long screensize = 0;
static lv_disp_drv_t disp_drv;
static lv_disp_draw_buf_t disp_buf;
static lv_color_t* buf1 = nullptr;
static lv_color_t* buf2 = nullptr;

// Flush callback for LVGL
static void fbdev_display_flush(lv_disp_drv_t* disp_drv, const lv_area_t* area, lv_color_t* color_p) {
    if (!fbp) return;

    int32_t x, y;
    for (y = area->y1; y <= area->y2; y++) {
        for (x = area->x1; x <= area->x2; x++) {
            long location = (x + vinfo.xoffset) * (vinfo.bits_per_pixel / 8) +
                           (y + vinfo.yoffset) * finfo.line_length;

            lv_color_t c = *color_p;

            if (vinfo.bits_per_pixel == 32) {
                *(fbp + location) = c.ch.blue;
                *(fbp + location + 1) = c.ch.green;
                *(fbp + location + 2) = c.ch.red;
                *(fbp + location + 3) = 0xFF;  // Alpha
            } else if (vinfo.bits_per_pixel == 16) {
                // RGB565
                uint16_t pixel = ((c.ch.red & 0xF8) << 8) |
                                ((c.ch.green & 0xFC) << 3) |
                                (c.ch.blue >> 3);
                *((uint16_t*)(fbp + location)) = pixel;
            }

            color_p++;
        }
    }

    lv_disp_flush_ready(disp_drv);
}

bool fbdev_display_init() {
    const char* fbdev = "/dev/fb0";

    // Open framebuffer device
    fbfd = open(fbdev, O_RDWR);
    if (fbfd == -1) {
        perror("[fbdev] Failed to open framebuffer device");
        return false;
    }

    // Get fixed screen information
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo) == -1) {
        perror("[fbdev] Failed to get fixed screen info");
        close(fbfd);
        return false;
    }

    // Get variable screen information
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        perror("[fbdev] Failed to get variable screen info");
        close(fbfd);
        return false;
    }

    // Calculate screen size
    screensize = vinfo.yres_virtual * finfo.line_length;

    // Map framebuffer to memory
    fbp = (uint8_t*)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    if (fbp == MAP_FAILED) {
        perror("[fbdev] Failed to map framebuffer");
        close(fbfd);
        return false;
    }

    // Initialize LVGL
    lv_init();

    // Allocate draw buffers
    uint32_t buf_size = vinfo.xres * 100;
    buf1 = new lv_color_t[buf_size];
    buf2 = new lv_color_t[buf_size];

    // Initialize display buffer
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, buf_size);

    // Initialize display driver
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = fbdev_display_flush;
    disp_drv.hor_res = vinfo.xres;
    disp_drv.ver_res = vinfo.yres;
    lv_disp_drv_register(&disp_drv);

    printf("[fbdev] Display initialized (%dx%d, %d bpp)\n",
           vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);
    return true;
}

void fbdev_display_update() {
    lv_timer_handler();
}

void fbdev_display_cleanup() {
    if (buf1) delete[] buf1;
    if (buf2) delete[] buf2;
    if (fbp && fbp != MAP_FAILED) {
        munmap(fbp, screensize);
    }
    if (fbfd >= 0) {
        close(fbfd);
    }
    printf("[fbdev] Display cleaned up\n");
}
