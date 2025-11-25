/**
 * @file main_lvgl_test.c
 * @brief SINGLE DISPLAY TEST MODE - Teste jedes Display einzeln
 *
 * ANLEITUNG:
 * 1. Wähle unten welches Display getestet werden soll (Zeile 28-31)
 * 2. Kompiliere: idf.py build
 * 3. Flash: idf.py -p COM4 flash monitor
 * 4. Das ausgewählte Display sollte einen roten Screen mit "TEST" Text zeigen
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"

#include "lvgl.h"
#include "lvgl_gc9a01_driver.h"

static const char *TAG = "DISPLAY-TEST";

/* ============================================================================
 * TEST KONFIGURATION - NUR EINE ZEILE AKTIVIEREN!
 * ========================================================================== */
// #define TEST_DISPLAY_1_CPU      1   // ← Aktiviert (1 = AN, 0 = AUS)
#define TEST_DISPLAY_2_GPU      1
// #define TEST_DISPLAY_3_RAM      1
// #define TEST_DISPLAY_4_NETWORK  1

/* ============================================================================
 * PIN DEFINITIONEN
 * ========================================================================== */
#ifdef TEST_DISPLAY_1_CPU
static const lvgl_gc9a01_config_t test_config = {
    .pin_sck = 4,
    .pin_mosi = 5,
    .pin_cs = 12,
    .pin_dc = 11,
    .pin_rst = 13,
    .spi_host = SPI2_HOST
};
static const char *DISPLAY_NAME = "CPU (Display 1)";
#endif

#ifdef TEST_DISPLAY_2_GPU
static const lvgl_gc9a01_config_t test_config = {
    .pin_sck = 4,
    .pin_mosi = 5,
    .pin_cs = 9,
    .pin_dc = 46,
    .pin_rst = 10,
    .spi_host = SPI2_HOST
};
static const char *DISPLAY_NAME = "GPU (Display 2)";
#endif

#ifdef TEST_DISPLAY_3_RAM
static const lvgl_gc9a01_config_t test_config = {
    .pin_sck = 4,
    .pin_mosi = 5,
    .pin_cs = 8,
    .pin_dc = 18,
    .pin_rst = 3,
    .spi_host = SPI2_HOST
};
static const char *DISPLAY_NAME = "RAM (Display 3)";
#endif

#ifdef TEST_DISPLAY_4_NETWORK
static const lvgl_gc9a01_config_t test_config = {
    .pin_sck = 4,
    .pin_mosi = 5,
    .pin_cs = 16,
    .pin_dc = 15,
    .pin_rst = 17,
    .spi_host = SPI2_HOST
};
static const char *DISPLAY_NAME = "Network (Display 4)";
#endif

/* ============================================================================
 * LVGL TASKS
 * ========================================================================== */
void lvgl_tick_task(void *arg)
{
    while (1) {
        lv_tick_inc(10);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void lvgl_timer_task(void *arg)
{
    while (1) {
        uint32_t time_till_next = lv_timer_handler();

        /* Yield to other tasks to prevent watchdog */
        if (time_till_next > 10) {
            vTaskDelay(pdMS_TO_TICKS(10));
        } else {
            vTaskDelay(pdMS_TO_TICKS(time_till_next));
        }
    }
}

/* ============================================================================
 * TEST SCREEN ERSTELLEN
 * ========================================================================== */
void create_test_screen(lv_display_t *disp)
{
    ESP_LOGI(TAG, "Creating test screen...");

    /* Screen erstellen */
    lv_obj_t *screen = lv_obj_create(NULL);

    /* ROTER Hintergrund - gut sichtbar! */
    lv_obj_set_style_bg_color(screen, lv_color_make(0xff, 0x00, 0x00), 0);

    /* GROßER Text in der Mitte */
    lv_obj_t *label = lv_label_create(screen);
    lv_label_set_text(label, "TEST\nOK");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_42, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    /* Screen laden */
    lv_screen_load(screen);

    ESP_LOGI(TAG, "Test screen created and loaded!");
}

/* ============================================================================
 * MAIN
 * ========================================================================== */
void app_main(void)
{
    ESP_LOGI(TAG, "=== SINGLE DISPLAY TEST MODE ===");
    ESP_LOGI(TAG, "Testing: %s", DISPLAY_NAME);

    /* Disable Task Watchdog for test mode */
    ESP_ERROR_CHECK(esp_task_wdt_deinit());
    ESP_LOGI(TAG, "Task Watchdog disabled for test");

    /* ========================================================================
     * STEP 1: SPI Bus initialisieren
     * ====================================================================== */
    spi_bus_config_t buscfg = {
        .mosi_io_num = 5,
        .miso_io_num = -1,
        .sclk_io_num = 4,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 240 * 240 * 2
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    ESP_LOGI(TAG, "SPI bus initialized");

    /* ========================================================================
     * STEP 2: LVGL initialisieren
     * ====================================================================== */
    ESP_LOGI(TAG, "Initializing LVGL...");
    lv_init();

    /* ========================================================================
     * STEP 3: Display initialisieren
     * ====================================================================== */
    ESP_LOGI(TAG, "Initializing display...");
    lvgl_gc9a01_handle_t display_handle;
    ESP_ERROR_CHECK(lvgl_gc9a01_init(&test_config, &display_handle));
    lv_display_t *disp = lvgl_gc9a01_get_display(&display_handle);
    ESP_LOGI(TAG, "Display initialized!");

    /* ========================================================================
     * STEP 4: Test Screen erstellen
     * ====================================================================== */
    create_test_screen(disp);

    /* ========================================================================
     * STEP 5: LVGL Tasks starten
     * ====================================================================== */
    xTaskCreate(lvgl_tick_task, "lvgl_tick", 2048, NULL, 10, NULL);
    xTaskCreatePinnedToCore(lvgl_timer_task, "lvgl_timer", 8192, NULL, 5, NULL, 1);  /* Core 1 */

    ESP_LOGI(TAG, "=== System ready! ===");
    ESP_LOGI(TAG, "You should see a RED screen with 'TEST OK' text");
    ESP_LOGI(TAG, "If screen is BLACK, check wiring!");
}
