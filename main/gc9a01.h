#ifndef GC9A01_H
#define GC9A01_H

#include <stdint.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"

// Display size
#define GC9A01_WIDTH  240
#define GC9A01_HEIGHT 240

// RGB565 Color macros
#define RGB565(r, g, b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

// Common colors
#define COLOR_BLACK       0x0000
#define COLOR_WHITE       0xFFFF
#define COLOR_RED         0xF800
#define COLOR_GREEN       0x07E0
#define COLOR_BLUE        0x001F
#define COLOR_CYAN        0x07FF
#define COLOR_MAGENTA     0xF81F
#define COLOR_YELLOW      0xFFE0
#define COLOR_ORANGE      0xFD20
#define COLOR_GRAY        0x8410
#define COLOR_DARKGRAY    0x4208

// Cyberpunk colors
#define COLOR_CYBER_CYAN      RGB565(0, 255, 255)
#define COLOR_CYBER_MAGENTA   RGB565(255, 0, 255)
#define COLOR_CYBER_BLUE      RGB565(67, 97, 238)
#define COLOR_CYBER_BG        RGB565(10, 10, 10)

// Display 1-4 GPIO pins
typedef struct {
    int sck;
    int mosi;
    int cs;
    int dc;
    int rst;
} gc9a01_pins_t;

// Display handle
typedef struct {
    spi_device_handle_t spi;
    int dc_pin;
    int rst_pin;
    int cs_pin;
} gc9a01_handle_t;

// Initialize display
esp_err_t gc9a01_init(gc9a01_handle_t *handle, const gc9a01_pins_t *pins, int spi_host);

// Basic drawing functions
void gc9a01_fill_screen(gc9a01_handle_t *handle, uint16_t color);
void gc9a01_fill_rect(gc9a01_handle_t *handle, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void gc9a01_draw_pixel(gc9a01_handle_t *handle, uint16_t x, uint16_t y, uint16_t color);
void gc9a01_draw_line(gc9a01_handle_t *handle, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
void gc9a01_draw_circle(gc9a01_handle_t *handle, uint16_t x0, uint16_t y0, uint16_t r, uint16_t color);
void gc9a01_draw_circle_filled(gc9a01_handle_t *handle, uint16_t x0, uint16_t y0, uint16_t r, uint16_t color);

// Advanced drawing
void gc9a01_draw_arc(gc9a01_handle_t *handle, uint16_t x0, uint16_t y0, uint16_t r, 
                     uint16_t start_angle, uint16_t end_angle, uint16_t thickness, uint16_t color);

#endif // GC9A01_H
