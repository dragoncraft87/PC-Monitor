/**
 * @file lvgl_gc9a01_driver.h
 * @brief LVGL Display Driver for GC9A01 (240x240 Round Display)
 *
 * Connects the GC9A01 hardware driver with LVGL display interface.
 * Supports multiple display instances with PSRAM frame buffers.
 */

#ifndef LVGL_GC9A01_DRIVER_H
#define LVGL_GC9A01_DRIVER_H

#include "lvgl.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/spi_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Display configuration structure
 */
typedef struct {
    int pin_sck;
    int pin_mosi;
    int pin_cs;
    int pin_dc;
    int pin_rst;
    spi_host_device_t spi_host;
} lvgl_gc9a01_config_t;

/**
 * @brief Display handle structure
 */
typedef struct {
    lv_display_t *lv_disp;
    esp_lcd_panel_handle_t panel_handle;
    void *draw_buf1;
    void *draw_buf2;
} lvgl_gc9a01_handle_t;

/**
 * @brief Initialize LVGL display with GC9A01
 *
 * @param config Display pin configuration
 * @param handle Output handle
 * @return esp_err_t ESP_OK on success
 */
esp_err_t lvgl_gc9a01_init(const lvgl_gc9a01_config_t *config, lvgl_gc9a01_handle_t *handle);

/**
 * @brief Get LVGL display object
 *
 * @param handle Display handle
 * @return lv_display_t* LVGL display object
 */
lv_display_t *lvgl_gc9a01_get_display(lvgl_gc9a01_handle_t *handle);

#ifdef __cplusplus
}
#endif

#endif /* LVGL_GC9A01_DRIVER_H */
