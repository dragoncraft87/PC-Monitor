/**
 * @file main_lvgl.c
 * @brief PC Monitor - Desert-Spec v2.0 (Modular Edition)
 *
 * Refactored modular architecture:
 * - storage/   : LittleFS, hw_identity, gui_settings
 * - drivers/   : usb_serial_comm
 * - ui/        : ui_manager, screensaver_mgr
 * - screens/   : screen implementations
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

#include "lvgl.h"
#include "lvgl_gc9a01_driver.h"

/* Modular includes */
#include "core/system_types.h"
#include "storage/storage_mgr.h"
#include "storage/hw_identity.h"
#include "gui_settings.h"
#include "drivers/usb_serial_comm.h"
#include "ui/ui_manager.h"
#include "ui/screensaver_mgr.h"
#include "screens/screens_lvgl.h"

static const char *TAG = "MAIN";

/* =============================================================================
 * CONFIGURATION
 * ========================================================================== */
#define SCREENSAVER_TIMEOUT_MS   30000   /* 30 seconds no data -> screensaver */
#define STALE_DATA_THRESHOLD_MS  2000    /* 2 seconds -> show red dot */
#define DISPLAY_UPDATE_MS        100     /* 10 FPS - Watchdog friendly */

/* =============================================================================
 * GLOBAL STATE
 * ========================================================================== */
static SemaphoreHandle_t s_stats_mutex = NULL;
static SemaphoreHandle_t s_lvgl_mutex = NULL;

/* Display Handles */
static lvgl_gc9a01_handle_t display_cpu, display_gpu, display_ram, display_network;

/* Screen Handles */
static ui_screens_t s_screens = {0};
static ui_screensavers_t s_screensavers = {0};
static ui_status_dots_t s_dots = {0};

/* SPI Pin Configurations */
static const lvgl_gc9a01_config_t config_cpu = {
    .pin_sck = 4, .pin_mosi = 5, .pin_cs = 12,
    .pin_dc = 11, .pin_rst = 13, .spi_host = SPI2_HOST
};
static const lvgl_gc9a01_config_t config_gpu = {
    .pin_sck = 4, .pin_mosi = 5, .pin_cs = 9,
    .pin_dc = 46, .pin_rst = 10, .spi_host = SPI2_HOST
};
static const lvgl_gc9a01_config_t config_ram = {
    .pin_sck = 4, .pin_mosi = 5, .pin_cs = 8,
    .pin_dc = 18, .pin_rst = 3, .spi_host = SPI2_HOST
};
static const lvgl_gc9a01_config_t config_net = {
    .pin_sck = 4, .pin_mosi = 5, .pin_cs = 16,
    .pin_dc = 15, .pin_rst = 17, .spi_host = SPI2_HOST
};

/* =============================================================================
 * SCREENSAVER THEME COLORS (from gui_settings)
 * ========================================================================== */
#define COLOR_SONIC_BG      lv_color_hex(gui_settings.ss_bg_color[SCREEN_CPU])
#define COLOR_ART_BG        lv_color_hex(gui_settings.ss_bg_color[SCREEN_GPU])
#define COLOR_DK_BG         lv_color_hex(gui_settings.ss_bg_color[SCREEN_RAM])
#define COLOR_PACMAN_BG     lv_color_hex(gui_settings.ss_bg_color[SCREEN_NET])

/* =============================================================================
 * TASK: Display Update - 10 FPS with Screensaver Logic
 * ========================================================================== */
static void display_update_task(void *arg)
{
    ESP_LOGI(TAG, "Display Update Task started (10 FPS)");

    while (1) {
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        uint32_t last_data = usb_serial_get_last_data_time();
        uint32_t time_since_data = now - last_data;

        bool data_is_stale = (time_since_data > STALE_DATA_THRESHOLD_MS);
        bool should_screensave = (time_since_data > SCREENSAVER_TIMEOUT_MS);

        if (xSemaphoreTake(s_lvgl_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {

            /* Screensaver logic */
            if (should_screensave && !ui_manager_is_screensaver_active()) {
                ui_manager_set_screensaver_active(true);
                ui_manager_show_screensavers(true);
                ESP_LOGW(TAG, "Screensaver ON (no data for %lu ms)", (unsigned long)time_since_data);
            }
            else if (!should_screensave && ui_manager_is_screensaver_active()) {
                ui_manager_set_screensaver_active(false);
                ui_manager_show_screensavers(false);
                ESP_LOGI(TAG, "Screensaver OFF (data received)");
            }

            /* Red dot logic */
            if (data_is_stale && !ui_manager_is_screensaver_active()) {
                ui_manager_show_status_dots(true);
            } else {
                ui_manager_show_status_dots(false);
            }

            /* Update screens (only if not in screensaver) */
            if (!ui_manager_is_screensaver_active()) {
                xSemaphoreTake(s_stats_mutex, portMAX_DELAY);
                pc_stats_t local_stats = *usb_serial_get_stats();
                xSemaphoreGive(s_stats_mutex);

                ui_manager_update_screens(&local_stats);
            }

            xSemaphoreGive(s_lvgl_mutex);
        }

        vTaskDelay(pdMS_TO_TICKS(DISPLAY_UPDATE_MS));
    }
}

/* =============================================================================
 * TASK: LVGL Tick (10ms)
 * ========================================================================== */
static void lvgl_tick_task(void *arg)
{
    while (1) {
        lv_tick_inc(10);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* =============================================================================
 * TASK: LVGL Timer Handler - Watchdog Friendly
 * ========================================================================== */
static void lvgl_timer_task(void *arg)
{
    ESP_LOGI(TAG, "LVGL Timer Task started");

    while (1) {
        if (xSemaphoreTake(s_lvgl_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            uint32_t time_till_next = lv_timer_handler();
            xSemaphoreGive(s_lvgl_mutex);

            uint32_t delay_ms = (time_till_next < 5) ? 5 : time_till_next;
            if (delay_ms > 30) delay_ms = 30;
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        } else {
            vTaskDelay(pdMS_TO_TICKS(5));
        }

        taskYIELD();
    }
}

/* =============================================================================
 * MAIN APPLICATION ENTRY
 * ========================================================================== */
void app_main(void)
{
    ESP_LOGI(TAG, "===========================================");
    ESP_LOGI(TAG, "PC Monitor - Desert-Spec v2.0");
    ESP_LOGI(TAG, "===========================================");

    /* Initialize LittleFS storage */
    if (storage_init() == ESP_OK) {
        hw_identity_load();
        gui_settings_load();
    } else {
        gui_settings_init_defaults(&gui_settings);
    }

    /* Create mutexes */
    s_stats_mutex = xSemaphoreCreateMutex();
    s_lvgl_mutex = xSemaphoreCreateMutex();

    if (!s_stats_mutex || !s_lvgl_mutex) {
        ESP_LOGE(TAG, "Failed to create mutexes!");
        return;
    }

    /* Initialize USB Serial */
    usb_serial_init();

    /* Register command handlers */
    usb_serial_register_handler(hw_identity_handle_command);
    usb_serial_register_handler(ui_manager_handle_color_command);
    usb_serial_register_handler(ss_image_handle_command);

    /* Initialize UI manager */
    ui_manager_init(s_lvgl_mutex);

    /* Initialize SPI Bus */
    spi_bus_config_t buscfg = {
        .mosi_io_num = 5,
        .sclk_io_num = 4,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 240 * 240 * 2 + 8
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    ESP_LOGI(TAG, "SPI Bus initialized");

    /* Initialize LVGL */
    lv_init();
    ESP_LOGI(TAG, "LVGL initialized");

    /* Initialize displays and screens (under mutex) */
    xSemaphoreTake(s_lvgl_mutex, portMAX_DELAY);

    /* Initialize screensaver image system */
    ss_images_init();

    hw_identity_t *hw_id = hw_identity_get();

    /* Display 1: CPU */
    ESP_LOGI(TAG, "Initializing CPU display...");
    lvgl_gc9a01_init(&config_cpu, &display_cpu);
    s_screens.cpu = screen_cpu_create(lvgl_gc9a01_get_display(&display_cpu));
    if (s_screens.cpu && s_screens.cpu->screen) {
        if (s_screens.cpu->label_title) {
            lv_label_set_text(s_screens.cpu->label_title, hw_id->cpu_name);
        }
        s_dots.cpu = ui_manager_create_status_dot(s_screens.cpu->screen);
        s_screensavers.cpu = ui_manager_create_screensaver(
            s_screens.cpu->screen, COLOR_SONIC_BG, ss_image_get_dsc(SS_IMG_CPU));
    }

    /* Display 2: GPU */
    ESP_LOGI(TAG, "Initializing GPU display...");
    lvgl_gc9a01_init(&config_gpu, &display_gpu);
    s_screens.gpu = screen_gpu_create(lvgl_gc9a01_get_display(&display_gpu));
    if (s_screens.gpu && s_screens.gpu->screen) {
        if (s_screens.gpu->label_title) {
            lv_label_set_text(s_screens.gpu->label_title, hw_id->gpu_name);
        }
        s_dots.gpu = ui_manager_create_status_dot(s_screens.gpu->screen);
        s_screensavers.gpu = ui_manager_create_screensaver(
            s_screens.gpu->screen, COLOR_ART_BG, ss_image_get_dsc(SS_IMG_GPU));
    }

    /* Display 3: RAM */
    ESP_LOGI(TAG, "Initializing RAM display...");
    lvgl_gc9a01_init(&config_ram, &display_ram);
    s_screens.ram = screen_ram_create(lvgl_gc9a01_get_display(&display_ram));
    if (s_screens.ram && s_screens.ram->screen) {
        s_dots.ram = ui_manager_create_status_dot(s_screens.ram->screen);
        s_screensavers.ram = ui_manager_create_screensaver(
            s_screens.ram->screen, COLOR_DK_BG, ss_image_get_dsc(SS_IMG_RAM));
    }

    /* Display 4: Network */
    ESP_LOGI(TAG, "Initializing Network display...");
    lvgl_gc9a01_init(&config_net, &display_network);
    s_screens.network = screen_network_create(lvgl_gc9a01_get_display(&display_network));
    if (s_screens.network && s_screens.network->screen) {
        s_dots.net = ui_manager_create_status_dot(s_screens.network->screen);
        s_screensavers.net = ui_manager_create_screensaver(
            s_screens.network->screen, COLOR_PACMAN_BG, ss_image_get_dsc(SS_IMG_NET));
    }

    /* Register UI handles with manager */
    ui_manager_set_screens(&s_screens);
    ui_manager_set_screensavers(&s_screensavers);
    ui_manager_set_status_dots(&s_dots);

    xSemaphoreGive(s_lvgl_mutex);
    ESP_LOGI(TAG, "All displays initialized");

    /* Start USB RX task */
    usb_serial_start_rx_task(s_stats_mutex);

    /* Create remaining tasks */
    xTaskCreate(lvgl_tick_task, "lv_tick", 2048, NULL, 2, NULL);
    xTaskCreatePinnedToCore(lvgl_timer_task, "lv_timer", 8192, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(display_update_task, "update", 4096, NULL, 2, NULL, 0);

    ESP_LOGI(TAG, "===========================================");
    ESP_LOGI(TAG, "System ready. Waiting for USB data...");
    ESP_LOGI(TAG, "Screensaver in %d seconds if no data", SCREENSAVER_TIMEOUT_MS / 1000);
    ESP_LOGI(TAG, "===========================================");
}
