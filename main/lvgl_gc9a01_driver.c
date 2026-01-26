/**
 * @file lvgl_gc9a01_driver.c
 * @brief LVGL Display Driver Implementation for GC9A01
 *
 * "Desert-Spec" Edition - Full Double Buffering in PSRAM
 *
 * Features:
 * - Full frame double buffering (240x240 pixels per buffer)
 * - Zero tearing/artifacts with LV_DISPLAY_RENDER_MODE_FULL
 * - PSRAM-based buffers for maximum memory efficiency
 * - DMA-capable swap buffer for fast SPI transfers
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

/* Full frame buffer size for zero-tearing double buffering */
#define GC9A01_FULL_BUF_SIZE (GC9A01_WIDTH * GC9A01_HEIGHT)  /* 57600 pixels = 115200 bytes per buffer */

/**
 * @brief Flush callback for LVGL - Full Frame Mode
 *
 * With full frame double buffering, LVGL renders to one buffer while
 * the other is being sent to the display. This eliminates ALL tearing
 * and partial-redraw artifacts ("Post-it" effects).
 *
 * Important: GC9A01 SPI LCD is big-endian, so we need to swap RGB565 byte order
 * before sending to display (LVGL uses little-endian internally)
 */
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    lvgl_gc9a01_handle_t *handle = lv_display_get_user_data(disp);

    /* Safety checks */
    if (!handle || !px_map || !area) {
        ESP_LOGE(TAG, "Flush callback: Invalid parameters!");
        if (disp) lv_display_flush_ready(disp);
        return;
    }

    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;

    /* Bounds check */
    if (offsetx1 < 0) offsetx1 = 0;
    if (offsety1 < 0) offsety1 = 0;
    if (offsetx2 >= GC9A01_WIDTH) offsetx2 = GC9A01_WIDTH - 1;
    if (offsety2 >= GC9A01_HEIGHT) offsety2 = GC9A01_HEIGHT - 1;

    /* Calculate pixel count */
    int width = offsetx2 - offsetx1 + 1;
    int height = offsety2 - offsety1 + 1;
    int pixel_count = width * height;
    size_t byte_size = pixel_count * sizeof(lv_color_t);

    /* Safety check for swap buffer size */
    if (byte_size > GC9A01_FULL_BUF_SIZE * sizeof(lv_color_t)) {
        ESP_LOGE(TAG, "Flush: Area too large! %d bytes > %d bytes max",
                 byte_size, GC9A01_FULL_BUF_SIZE * sizeof(lv_color_t));
        lv_display_flush_ready(disp);
        return;
    }

    /* Copy to swap buffer and perform byte swap for big-endian display */
    memcpy(handle->swap_buf, px_map, byte_size);
    lv_draw_sw_rgb565_swap(handle->swap_buf, pixel_count);

    /* Send swapped data to display via DMA */
    esp_err_t ret = esp_lcd_panel_draw_bitmap(handle->panel_handle,
                                               offsetx1, offsety1,
                                               offsetx2 + 1, offsety2 + 1,
                                               handle->swap_buf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Flush: SPI transfer failed! Error: 0x%x", ret);
    }

    /* Inform LVGL that flushing is done */
    lv_display_flush_ready(disp);
}

/**
 * @brief Initialize LVGL display with GC9A01 - Full Double Buffering Mode
 *
 * This function allocates two full-frame buffers in PSRAM for true
 * double buffering. While LVGL renders to one buffer, the other is
 * sent to the display, eliminating all visual artifacts.
 *
 * Memory usage per display:
 * - 2x draw buffers: 2 * 115200 = 230400 bytes (PSRAM)
 * - 1x swap buffer:  115200 bytes (Internal DMA or PSRAM fallback)
 * - Total: ~345 KB per display, ~1.4 MB for 4 displays
 *
 * With 8MB PSRAM, this leaves plenty of room for LVGL objects.
 */
esp_err_t lvgl_gc9a01_init(const lvgl_gc9a01_config_t *config, lvgl_gc9a01_handle_t *handle)
{
    ESP_LOGI(TAG, "Initializing GC9A01 display (CS=%d, DC=%d, RST=%d)",
             config->pin_cs, config->pin_dc, config->pin_rst);
    ESP_LOGI(TAG, "Mode: FULL DOUBLE BUFFERING (240x240 per buffer)");

    /* ========================================================================
     * STEP 1: Configure SPI Device Interface
     * ====================================================================== */
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = config->pin_dc,
        .cs_gpio_num = config->pin_cs,
        .pclk_hz = 40 * 1000 * 1000,  /* 40 MHz SPI clock for fast transfers */
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
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,  /* GC9A01 uses BGR order */
        .bits_per_pixel = 16,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &handle->panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(handle->panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(handle->panel_handle));

    /* Display orientation correction (anti-mirroring) */
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(handle->panel_handle, true, false));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(handle->panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(handle->panel_handle, true));

    /* ========================================================================
     * STEP 3: Allocate FULL FRAME Double Buffers in PSRAM
     * ====================================================================== */
    size_t buf_size_bytes = GC9A01_FULL_BUF_SIZE * sizeof(lv_color_t);

    ESP_LOGI(TAG, "Allocating FULL FRAME buffers (%d pixels = %d bytes each)...",
             GC9A01_FULL_BUF_SIZE, buf_size_bytes);

    /* Draw buffers in PSRAM (for LVGL rendering) */
    handle->draw_buf1 = heap_caps_malloc(buf_size_bytes, MALLOC_CAP_SPIRAM);
    handle->draw_buf2 = heap_caps_malloc(buf_size_bytes, MALLOC_CAP_SPIRAM);

    /* Swap buffer - try internal DMA RAM first (fastest for SPI), fallback to PSRAM */
    handle->swap_buf = heap_caps_malloc(buf_size_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!handle->swap_buf) {
        ESP_LOGW(TAG, "Internal DMA RAM full, using PSRAM for swap buffer (slightly slower)");
        handle->swap_buf = heap_caps_malloc(buf_size_bytes, MALLOC_CAP_SPIRAM);
    }

    /* Verify all buffers allocated */
    if (!handle->draw_buf1 || !handle->draw_buf2 || !handle->swap_buf) {
        ESP_LOGE(TAG, "Failed to allocate full frame buffers!");
        ESP_LOGE(TAG, "  draw_buf1=%p, draw_buf2=%p, swap_buf=%p",
                 handle->draw_buf1, handle->draw_buf2, handle->swap_buf);
        ESP_LOGE(TAG, "  Free PSRAM: %d bytes", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        ESP_LOGE(TAG, "  Free Internal: %d bytes", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

        /* Cleanup partial allocations */
        if (handle->draw_buf1) heap_caps_free(handle->draw_buf1);
        if (handle->draw_buf2) heap_caps_free(handle->draw_buf2);
        if (handle->swap_buf) heap_caps_free(handle->swap_buf);

        return ESP_ERR_NO_MEM;
    }

    /* Clear buffers to black */
    memset(handle->draw_buf1, 0, buf_size_bytes);
    memset(handle->draw_buf2, 0, buf_size_bytes);
    memset(handle->swap_buf, 0, buf_size_bytes);

    ESP_LOGI(TAG, "Full frame buffers allocated:");
    ESP_LOGI(TAG, "  draw_buf1=%p (PSRAM)", handle->draw_buf1);
    ESP_LOGI(TAG, "  draw_buf2=%p (PSRAM)", handle->draw_buf2);
    ESP_LOGI(TAG, "  swap_buf=%p", handle->swap_buf);

    /* ========================================================================
     * STEP 4: Create LVGL Display with FULL Render Mode
     * ====================================================================== */
    handle->lv_disp = lv_display_create(GC9A01_WIDTH, GC9A01_HEIGHT);
    if (!handle->lv_disp) {
        ESP_LOGE(TAG, "Failed to create LVGL display!");
        heap_caps_free(handle->draw_buf1);
        heap_caps_free(handle->draw_buf2);
        heap_caps_free(handle->swap_buf);
        return ESP_FAIL;
    }

    /* Set FULL FRAME double buffers with FULL render mode
     *
     * LV_DISPLAY_RENDER_MODE_FULL means:
     * - LVGL always redraws the entire screen
     * - One buffer is rendered while the other is being sent
     * - NO tearing, NO partial redraw artifacts
     * - Perfect for displays where visual quality is paramount
     */
    lv_display_set_buffers(handle->lv_disp,
                           handle->draw_buf1,
                           handle->draw_buf2,
                           buf_size_bytes,
                           LV_DISPLAY_RENDER_MODE_FULL);

    /* Set flush callback */
    lv_display_set_flush_cb(handle->lv_disp, lvgl_flush_cb);

    /* Store handle as user data (needed in flush callback) */
    lv_display_set_user_data(handle->lv_disp, handle);

    /* Set as default display temporarily */
    lv_display_set_default(handle->lv_disp);

    ESP_LOGI(TAG, "GC9A01 display initialized - FULL DOUBLE BUFFERING active");
    ESP_LOGI(TAG, "Memory used: %d bytes (%d KB)", buf_size_bytes * 3, (buf_size_bytes * 3) / 1024);

    return ESP_OK;
}

/**
 * @brief Get LVGL display object from handle
 */
lv_display_t *lvgl_gc9a01_get_display(lvgl_gc9a01_handle_t *handle)
{
    return handle->lv_disp;
}
