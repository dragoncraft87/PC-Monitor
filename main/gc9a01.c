#include "gc9a01.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <math.h>

static const char *TAG = "GC9A01";

// GC9A01 Commands
#define GC9A01_SLPOUT   0x11
#define GC9A01_DISPON   0x29
#define GC9A01_CASET    0x2A
#define GC9A01_RASET    0x2B
#define GC9A01_RAMWR    0x2C
#define GC9A01_MADCTL   0x36
#define GC9A01_COLMOD   0x3A

static void gc9a01_send_cmd(gc9a01_handle_t *handle, uint8_t cmd) {
    gpio_set_level(handle->dc_pin, 0);
    spi_transaction_t trans = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    spi_device_transmit(handle->spi, &trans);
}

static void gc9a01_send_data(gc9a01_handle_t *handle, uint8_t data) {
    gpio_set_level(handle->dc_pin, 1);
    spi_transaction_t trans = {
        .length = 8,
        .tx_buffer = &data,
    };
    spi_device_transmit(handle->spi, &trans);
}

static void gc9a01_send_data_buffer(gc9a01_handle_t *handle, const uint8_t *data, size_t len) {
    gpio_set_level(handle->dc_pin, 1);
    spi_transaction_t trans = {
        .length = len * 8,
        .tx_buffer = data,
    };
    spi_device_transmit(handle->spi, &trans);
}

esp_err_t gc9a01_init(gc9a01_handle_t *handle, const gc9a01_pins_t *pins, int spi_host) {
    // Configure GPIO pins
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pins->dc) | (1ULL << pins->rst) | (1ULL << pins->cs),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);
    
    handle->dc_pin = pins->dc;
    handle->rst_pin = pins->rst;
    handle->cs_pin = pins->cs;
    
    // Hardware reset
    gpio_set_level(pins->rst, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(pins->rst, 1);
    vTaskDelay(pdMS_TO_TICKS(120));
    
    // SPI device configuration
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 40 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = pins->cs,
        .queue_size = 7,
    };
    
    ESP_ERROR_CHECK(spi_bus_add_device(spi_host, &devcfg, &handle->spi));
    
    ESP_LOGI(TAG, "Starting GC9A01 initialization sequence...");
    
    // ===== VOLLSTÄNDIGE GC9A01 INIT SEQUENZ =====
    
    // Inter Register Enable1
    gc9a01_send_cmd(handle, 0xEF);
    
    // Inter Register Enable2
    gc9a01_send_cmd(handle, 0xEB);
    gc9a01_send_data(handle, 0x14);
    
    // Display Function Control
    gc9a01_send_cmd(handle, 0x84);
    gc9a01_send_data(handle, 0x40);
    
    gc9a01_send_cmd(handle, 0x85);
    gc9a01_send_data(handle, 0xFF);
    
    gc9a01_send_cmd(handle, 0x86);
    gc9a01_send_data(handle, 0xFF);
    
    gc9a01_send_cmd(handle, 0x87);
    gc9a01_send_data(handle, 0xFF);
    
    gc9a01_send_cmd(handle, 0x88);
    gc9a01_send_data(handle, 0x0A);
    
    gc9a01_send_cmd(handle, 0x89);
    gc9a01_send_data(handle, 0x21);
    
    gc9a01_send_cmd(handle, 0x8A);
    gc9a01_send_data(handle, 0x00);
    
    gc9a01_send_cmd(handle, 0x8B);
    gc9a01_send_data(handle, 0x80);
    
    gc9a01_send_cmd(handle, 0x8C);
    gc9a01_send_data(handle, 0x01);
    
    gc9a01_send_cmd(handle, 0x8D);
    gc9a01_send_data(handle, 0x01);
    
    gc9a01_send_cmd(handle, 0x8E);
    gc9a01_send_data(handle, 0xFF);
    
    gc9a01_send_cmd(handle, 0x8F);
    gc9a01_send_data(handle, 0xFF);
    
    // Display Function Control
    gc9a01_send_cmd(handle, 0xB6);
    gc9a01_send_data(handle, 0x00);
    gc9a01_send_data(handle, 0x20);
    
    // Memory Access Control (Rotation)
    gc9a01_send_cmd(handle, 0x36);
    gc9a01_send_data(handle, 0x08);
    
    // Pixel Format Set (16bit/pixel RGB565)
    gc9a01_send_cmd(handle, 0x3A);
    gc9a01_send_data(handle, 0x05);
    
    // Frame Rate Control
    gc9a01_send_cmd(handle, 0x90);
    gc9a01_send_data(handle, 0x08);
    gc9a01_send_data(handle, 0x08);
    gc9a01_send_data(handle, 0x08);
    gc9a01_send_data(handle, 0x08);
    
    // Display Inversion Control
    gc9a01_send_cmd(handle, 0xBD);
    gc9a01_send_data(handle, 0x06);
    
    // RGB Interface Signal Control
    gc9a01_send_cmd(handle, 0xBC);
    gc9a01_send_data(handle, 0x00);
    
    // Power Control 1
    gc9a01_send_cmd(handle, 0xC3);
    gc9a01_send_data(handle, 0x13);
    
    // Power Control 2
    gc9a01_send_cmd(handle, 0xC4);
    gc9a01_send_data(handle, 0x13);
    
    // Power Control 3
    gc9a01_send_cmd(handle, 0xC9);
    gc9a01_send_data(handle, 0x22);
    
    // VCOM Control
    gc9a01_send_cmd(handle, 0xBE);
    gc9a01_send_data(handle, 0x11);
    
    // Negative Voltage Gamma Control
    gc9a01_send_cmd(handle, 0xE1);
    gc9a01_send_data(handle, 0x10);
    gc9a01_send_data(handle, 0x0E);
    
    // Positive Voltage Gamma Control
    gc9a01_send_cmd(handle, 0xDF);
    gc9a01_send_data(handle, 0x21);
    gc9a01_send_data(handle, 0x0c);
    gc9a01_send_data(handle, 0x02);
    
    // Gamma Set
    gc9a01_send_cmd(handle, 0xF0);
    gc9a01_send_data(handle, 0x45);
    gc9a01_send_data(handle, 0x09);
    gc9a01_send_data(handle, 0x08);
    gc9a01_send_data(handle, 0x08);
    gc9a01_send_data(handle, 0x26);
    gc9a01_send_data(handle, 0x2A);
    
    gc9a01_send_cmd(handle, 0xF1);
    gc9a01_send_data(handle, 0x43);
    gc9a01_send_data(handle, 0x70);
    gc9a01_send_data(handle, 0x72);
    gc9a01_send_data(handle, 0x36);
    gc9a01_send_data(handle, 0x37);
    gc9a01_send_data(handle, 0x6F);
    
    gc9a01_send_cmd(handle, 0xF2);
    gc9a01_send_data(handle, 0x45);
    gc9a01_send_data(handle, 0x09);
    gc9a01_send_data(handle, 0x08);
    gc9a01_send_data(handle, 0x08);
    gc9a01_send_data(handle, 0x26);
    gc9a01_send_data(handle, 0x2A);
    
    gc9a01_send_cmd(handle, 0xF3);
    gc9a01_send_data(handle, 0x43);
    gc9a01_send_data(handle, 0x70);
    gc9a01_send_data(handle, 0x72);
    gc9a01_send_data(handle, 0x36);
    gc9a01_send_data(handle, 0x37);
    gc9a01_send_data(handle, 0x6F);
    
    // Sleep Out
    gc9a01_send_cmd(handle, 0x11);
    vTaskDelay(pdMS_TO_TICKS(120));
    
    // Display ON
    gc9a01_send_cmd(handle, 0x29);
    vTaskDelay(pdMS_TO_TICKS(20));
    
    ESP_LOGI(TAG, "GC9A01 initialized successfully!");
    return ESP_OK;
}

static void gc9a01_set_window(gc9a01_handle_t *handle, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    gc9a01_send_cmd(handle, GC9A01_CASET);
    gc9a01_send_data(handle, x0 >> 8);
    gc9a01_send_data(handle, x0 & 0xFF);
    gc9a01_send_data(handle, x1 >> 8);
    gc9a01_send_data(handle, x1 & 0xFF);
    
    gc9a01_send_cmd(handle, GC9A01_RASET);
    gc9a01_send_data(handle, y0 >> 8);
    gc9a01_send_data(handle, y0 & 0xFF);
    gc9a01_send_data(handle, y1 >> 8);
    gc9a01_send_data(handle, y1 & 0xFF);
    
    gc9a01_send_cmd(handle, GC9A01_RAMWR);
}

void gc9a01_fill_screen(gc9a01_handle_t *handle, uint16_t color) {
    gc9a01_fill_rect(handle, 0, 0, GC9A01_WIDTH, GC9A01_HEIGHT, color);
}

void gc9a01_fill_rect(gc9a01_handle_t *handle, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (x >= GC9A01_WIDTH || y >= GC9A01_HEIGHT) return;
    if (x + w > GC9A01_WIDTH) w = GC9A01_WIDTH - x;
    if (y + h > GC9A01_HEIGHT) h = GC9A01_HEIGHT - y;
    
    gc9a01_set_window(handle, x, y, x + w - 1, y + h - 1);
    
    uint32_t total_pixels = w * h;
    uint16_t color_swapped = (color >> 8) | (color << 8);
    
    // Send pixels in chunks
    #define CHUNK_SIZE 1024
    uint16_t buffer[CHUNK_SIZE];
    for (int i = 0; i < CHUNK_SIZE; i++) {
        buffer[i] = color_swapped;
    }
    
    gpio_set_level(handle->dc_pin, 1);
    while (total_pixels > 0) {
        uint32_t chunk = (total_pixels > CHUNK_SIZE) ? CHUNK_SIZE : total_pixels;
        spi_transaction_t trans = {
            .length = chunk * 16,
            .tx_buffer = buffer,
        };
        spi_device_transmit(handle->spi, &trans);
        total_pixels -= chunk;
    }
}

void gc9a01_draw_pixel(gc9a01_handle_t *handle, uint16_t x, uint16_t y, uint16_t color) {
    if (x >= GC9A01_WIDTH || y >= GC9A01_HEIGHT) return;
    
    gc9a01_set_window(handle, x, y, x, y);

    // Füge HIER die Vertauschung ein (gleiche Logik wie in fill_rect)
    uint16_t color_swapped = (color >> 8) | (color << 8); 

    uint8_t data[2] = {color_swapped >> 8, color_swapped & 0xFF};
    gc9a01_send_data_buffer(handle, data, 2);
}

void gc9a01_draw_line(gc9a01_handle_t *handle, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color) {
    int16_t dx = abs(x1 - x0);
    int16_t dy = abs(y1 - y0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;
    
    while (1) {
        gc9a01_draw_pixel(handle, x0, y0, color);
        
        if (x0 == x1 && y0 == y1) break;
        
        int16_t e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void gc9a01_draw_circle(gc9a01_handle_t *handle, uint16_t x0, uint16_t y0, uint16_t r, uint16_t color) {
    int16_t x = r;
    int16_t y = 0;
    int16_t err = 0;
    
    while (x >= y) {
        gc9a01_draw_pixel(handle, x0 + x, y0 + y, color);
        gc9a01_draw_pixel(handle, x0 + y, y0 + x, color);
        gc9a01_draw_pixel(handle, x0 - y, y0 + x, color);
        gc9a01_draw_pixel(handle, x0 - x, y0 + y, color);
        gc9a01_draw_pixel(handle, x0 - x, y0 - y, color);
        gc9a01_draw_pixel(handle, x0 - y, y0 - x, color);
        gc9a01_draw_pixel(handle, x0 + y, y0 - x, color);
        gc9a01_draw_pixel(handle, x0 + x, y0 - y, color);
        
        if (err <= 0) {
            y += 1;
            err += 2*y + 1;
        }
        if (err > 0) {
            x -= 1;
            err -= 2*x + 1;
        }
    }
}

void gc9a01_draw_circle_filled(gc9a01_handle_t *handle, uint16_t x0, uint16_t y0, uint16_t r, uint16_t color) {
    for (int16_t y = -r; y <= r; y++) {
        for (int16_t x = -r; x <= r; x++) {
            if (x*x + y*y <= r*r) {
                gc9a01_draw_pixel(handle, x0 + x, y0 + y, color);
            }
        }
    }
}

void gc9a01_draw_arc(gc9a01_handle_t *handle, uint16_t x0, uint16_t y0, uint16_t r,
                     uint16_t start_angle, uint16_t end_angle, uint16_t thickness, uint16_t color) {
    for (float angle = start_angle; angle <= end_angle; angle += 0.5f) {
        float rad = angle * M_PI / 180.0f;
        for (uint16_t t = 0; t < thickness; t++) {
            int16_t x = x0 + (int16_t)((r - t) * cos(rad));
            int16_t y = y0 + (int16_t)((r - t) * sin(rad));
            gc9a01_draw_pixel(handle, x, y, color);
        }
    }
}
