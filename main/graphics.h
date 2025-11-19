#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "gc9a01.h"

// Text sizes
typedef enum {
    FONT_SMALL = 1,   // 8x8
    FONT_MEDIUM = 2,  // 16x16
    FONT_LARGE = 3,   // 24x24
    FONT_XLARGE = 4   // 32x32
} font_size_t;

// Text functions
void graphics_draw_char(gc9a01_handle_t *display, uint16_t x, uint16_t y, char c, uint16_t color, font_size_t size);
void graphics_draw_string(gc9a01_handle_t *display, uint16_t x, uint16_t y, const char *str, uint16_t color, font_size_t size);
void graphics_draw_string_centered(gc9a01_handle_t *display, uint16_t y, const char *str, uint16_t color, font_size_t size);

// Gauge drawing (for CPU/GPU rings)
void graphics_draw_ring_gauge(gc9a01_handle_t *display, uint16_t cx, uint16_t cy, uint16_t radius,
                              uint16_t thickness, uint8_t percentage, uint16_t color_start, uint16_t color_end);

// Bar drawing (for RAM)
void graphics_draw_progress_bar(gc9a01_handle_t *display, uint16_t x, uint16_t y, uint16_t width,
                                uint16_t height, uint8_t percentage, uint16_t color);

// Number drawing helper
void graphics_draw_number(gc9a01_handle_t *display, uint16_t x, uint16_t y, int32_t number, uint16_t color, font_size_t size);

#endif // GRAPHICS_H
