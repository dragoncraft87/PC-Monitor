#include "graphics.h"
#include "bitmap_fonts.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

void graphics_draw_char(gc9a01_handle_t *display, uint16_t x, uint16_t y, char c, uint16_t color, font_size_t size) {
    int char_idx = get_char_index(c);
    const uint8_t *bitmap;
    int width, height;
    
    // Select font based on size
    switch (size) {
        case FONT_SMALL:
            bitmap = font_8x8[char_idx];
            width = FONT_WIDTH_8;
            height = FONT_HEIGHT_8;
            break;
        case FONT_MEDIUM:
            bitmap = font_8x16[char_idx];
            width = FONT_WIDTH_16;
            height = FONT_HEIGHT_16;
            break;
        case FONT_LARGE:
        case FONT_XLARGE:
        default:
            // For now, use 8x16 scaled for larger sizes
            bitmap = font_8x16[char_idx];
            width = FONT_WIDTH_16;
            height = FONT_HEIGHT_16;
            break;
    }
    
    // Draw bitmap
    for (int row = 0; row < height; row++) {
        uint8_t line = bitmap[row];
        for (int col = 0; col < 8; col++) {
            if (line & (0x80 >> col)) {
                // Scale for larger fonts
                int scale = (size == FONT_LARGE) ? 2 : (size == FONT_XLARGE) ? 3 : 1;
                
                for (int sy = 0; sy < scale; sy++) {
                    for (int sx = 0; sx < scale; sx++) {
                        gc9a01_draw_pixel(display, 
                                        x + col * scale + sx, 
                                        y + row * scale + sy, 
                                        color);
                    }
                }
            }
        }
    }
}

void graphics_draw_string(gc9a01_handle_t *display, uint16_t x, uint16_t y, const char *str, uint16_t color, font_size_t size) {
    uint16_t cursor_x = x;
    int scale = (size == FONT_LARGE) ? 2 : (size == FONT_XLARGE) ? 3 : 1;
    int char_width = (size <= FONT_MEDIUM) ? 9 : 9 * scale; // 8px + 1px spacing
    
    for (int i = 0; str[i] != '\0'; i++) {
        graphics_draw_char(display, cursor_x, y, str[i], color, size);
        cursor_x += char_width;
    }
}

void graphics_draw_string_centered(gc9a01_handle_t *display, uint16_t y, const char *str, uint16_t color, font_size_t size) {
    int scale = (size == FONT_LARGE) ? 2 : (size == FONT_XLARGE) ? 3 : 1;
    int char_width = (size <= FONT_MEDIUM) ? 9 : 9 * scale;
    uint16_t str_width = strlen(str) * char_width;
    uint16_t x = (GC9A01_WIDTH - str_width) / 2;
    
    graphics_draw_string(display, x, y, str, color, size);
}

void graphics_draw_ring_gauge(gc9a01_handle_t *display, uint16_t cx, uint16_t cy, uint16_t radius,
                              uint16_t thickness, uint8_t percentage, uint16_t color_start, uint16_t color_end) {
    // Draw a ring gauge (like a tachometer)
    // Start at top (270 degrees) and go clockwise
    
    float start_angle = 135.0f; // Bottom-left
    float sweep_angle = 270.0f; // Full circle sweep
    float end_angle = start_angle + (sweep_angle * percentage / 100.0f);
    
    // Draw the arc
    for (float angle = start_angle; angle < end_angle; angle += 0.5f) {
        float rad = angle * M_PI / 180.0f;
        
        // Draw thick arc by drawing multiple circles
        for (uint16_t t = 0; t < thickness; t++) {
            int16_t x = cx + (int16_t)((radius - t) * cos(rad));
            int16_t y = cy + (int16_t)((radius - t) * sin(rad));
            
            // Interpolate color
            float progress = (angle - start_angle) / (end_angle - start_angle);
            uint16_t r = ((color_start >> 11) & 0x1F) + (int)(progress * (((color_end >> 11) & 0x1F) - ((color_start >> 11) & 0x1F)));
            uint16_t g = ((color_start >> 5) & 0x3F) + (int)(progress * (((color_end >> 5) & 0x3F) - ((color_start >> 5) & 0x3F)));
            uint16_t b = (color_start & 0x1F) + (int)(progress * ((color_end & 0x1F) - (color_start & 0x1F)));
            uint16_t color = (r << 11) | (g << 5) | b;
            
            if (x >= 0 && x < GC9A01_WIDTH && y >= 0 && y < GC9A01_HEIGHT) {
                gc9a01_draw_pixel(display, x, y, color);
            }
        }
    }
}

void graphics_draw_progress_bar(gc9a01_handle_t *display, uint16_t x, uint16_t y, uint16_t width,
                                uint16_t height, uint8_t percentage, uint16_t color) {
    // Draw filled bar
    uint16_t filled_width = (width * percentage) / 100;
    gc9a01_fill_rect(display, x, y, filled_width, height, color);
}

void graphics_draw_number(gc9a01_handle_t *display, uint16_t x, uint16_t y, int32_t number, uint16_t color, font_size_t size) {
    char num_str[16];
    snprintf(num_str, sizeof(num_str), "%d", (int)number);
    graphics_draw_string(display, x, y, num_str, color, size);
}
