#include "lvgl_gc9a01_driver.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"

static const char *TAG = "LVGL_GC9A01";

// Callback: LVGL will Pixel senden
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    lvgl_gc9a01_handle_t *handle = (lvgl_gc9a01_handle_t *)lv_display_get_user_data(disp);
    int x1 = area->x1;
    int x2 = area->x2;
    int y1 = area->y1;
    int y2 = area->y2;

    // Sende Daten per SPI (blockiert, bis Queue frei ist -> Sicher bei 4 Displays)
    esp_lcd_panel_draw_bitmap(handle->panel_handle, x1, y1, x2 + 1, y2 + 1, px_map);

    // Melde LVGL sofort Vollzug
    lv_display_flush_ready(disp);
}

esp_err_t lvgl_gc9a01_init(const lvgl_gc9a01_config_t *config, lvgl_gc9a01_handle_t *handle)
{
    ESP_LOGI(TAG, "Initializing GC9A01 (CS=%d)", config->pin_cs);

    // 1. IO Config
    // DESERT SPEC: 20 MHz Takt für saubere Signale und weniger SPI-Fehler
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = config->pin_dc,
        .cs_gpio_num = config->pin_cs,
        .pclk_hz = 20 * 1000 * 1000, // 20 MHz
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)config->spi_host, &io_config, &handle->io_handle));

    // 2. Panel Config
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = config->pin_rst,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(handle->io_handle, &panel_config, &handle->panel_handle));
    
    esp_lcd_panel_reset(handle->panel_handle);
    esp_lcd_panel_init(handle->panel_handle);
    esp_lcd_panel_invert_color(handle->panel_handle, true);
    esp_lcd_panel_mirror(handle->panel_handle, true, true);
    esp_lcd_panel_disp_on_off(handle->panel_handle, true);

    // 3. LVGL Display
    handle->display = lv_display_create(240, 240);
    lv_display_set_user_data(handle->display, handle);
    lv_display_set_flush_cb(handle->display, lvgl_flush_cb);

    // 4. Buffer (Double Buffering im PSRAM)
    size_t buf_size = 240 * 240 * 2; // Full Frame 16bit
    handle->draw_buf1 = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
    handle->draw_buf2 = heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);

    if (!handle->draw_buf1 || !handle->draw_buf2) {
        ESP_LOGE(TAG, "PSRAM full! Could not allocate buffers.");
        return ESP_ERR_NO_MEM;
    }

    lv_display_set_buffers(handle->display, handle->draw_buf1, handle->draw_buf2, buf_size, LV_DISPLAY_RENDER_MODE_FULL);

    return ESP_OK;
}