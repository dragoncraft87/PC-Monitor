/**
 * @file lvgl_gc9a01_driver.c
 * @brief LVGL Display Driver Implementation for GC9A01
 */

#include "lvgl_gc9a01_driver.h"
#include "esp_lcd_gc9a01.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include <string.h>

static const char *TAG = "LVGL_GC9A01";

#define GC9A01_WIDTH  240
#define GC9A01_HEIGHT 240
#define GC9A01_BUF_SIZE (GC9A01_WIDTH * 40) /* 40 lines buffer */

/**
 * @brief Flush callback for LVGL
 */
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(disp);

    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;

    // Send pixel data to display
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map);

    // Inform LVGL that flushing is done
    lv_display_flush_ready(disp);
}

esp_err_t lvgl_gc9a01_init(const lvgl_gc9a01_config_t *config, lvgl_gc9a01_handle_t *handle)
{
    ESP_LOGI(TAG, "Initializing GC9A01 display (CS=%d, DC=%d, RST=%d)",
             config->pin_cs, config->pin_dc, config->pin_rst);

    /* ========================================================================
     * STEP 1: Configure SPI Device Interface
     * ====================================================================== */
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = config->pin_dc,
        .cs_gpio_num = config->pin_cs,
        .pclk_hz = 40 * 1000 * 1000,  /* 40 MHz SPI clock */
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)config->spi_host, &io_config, &io_handle));

    /* ========================================================================
     * STEP 2: Install GC9A01 Panel Driver
     * ====================================================================== */
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = config->pin_rst,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &handle->panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(handle->panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(handle->panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(handle->panel_handle, true));

    /* ========================================================================
     * STEP 3: Allocate LVGL Draw Buffers in PSRAM
     * ====================================================================== */
    ESP_LOGI(TAG, "Allocating draw buffers in PSRAM (%d bytes each)", GC9A01_BUF_SIZE * 2);

    handle->draw_buf1 = heap_caps_malloc(GC9A01_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    handle->draw_buf2 = heap_caps_malloc(GC9A01_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);

    if (!handle->draw_buf1 || !handle->draw_buf2) {
        ESP_LOGE(TAG, "Failed to allocate draw buffers in PSRAM!");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Draw buffers allocated: buf1=%p, buf2=%p", handle->draw_buf1, handle->draw_buf2);

    /* ========================================================================
     * STEP 4: Create LVGL Display
     * ====================================================================== */
    handle->lv_disp = lv_display_create(GC9A01_WIDTH, GC9A01_HEIGHT);
    if (!handle->lv_disp) {
        ESP_LOGE(TAG, "Failed to create LVGL display!");
        return ESP_FAIL;
    }

    /* Set draw buffers */
    lv_display_set_buffers(handle->lv_disp, handle->draw_buf1, handle->draw_buf2,
                           GC9A01_BUF_SIZE * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);

    /* Set flush callback */
    lv_display_set_flush_cb(handle->lv_disp, lvgl_flush_cb);

    /* Store panel handle as user data */
    lv_display_set_user_data(handle->lv_disp, handle->panel_handle);

    /* Round display - enable circle cropping */
    lv_display_set_default(handle->lv_disp);

    ESP_LOGI(TAG, "GC9A01 display initialized successfully");

    return ESP_OK;
}

lv_display_t *lvgl_gc9a01_get_display(lvgl_gc9a01_handle_t *handle)
{
    return handle->lv_disp;
}
