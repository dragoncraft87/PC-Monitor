/**
 * @file main_lvgl.c
 * @brief PC Monitor - Desert-Spec Phase 2 Edition
 *
 * Key Features:
 * - Robust line-buffer parser (no partial line processing)
 * - 100ms display update (10 FPS, Watchdog-friendly)
 * - Red dot indicator when data is stale (>1s)
 * - Screensaver after 5s no data (retro game themes)
 * - Crash-resistant design throughout
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/usb_serial_jtag.h"

#include "lvgl.h"
#include "lvgl_gc9a01_driver.h"
#include "screens/screens_lvgl.h"

// Include screen implementations
#include "screens/screen_cpu_lvgl.c"
#include "screens/screen_gpu_lvgl.c"
#include "screens/screen_ram_lvgl.c"
#include "screens/screen_network_lvgl.c"

static const char *TAG = "PC-MONITOR";

/* =============================================================================
 * CONFIGURATION - Desert-Spec Phase 2
 * ========================================================================== */
#define SCREENSAVER_TIMEOUT_MS   30000  // 30 seconds no data -> screensaver
#define STALE_DATA_THRESHOLD_MS  2000   // 2 seconds -> show red dot
#define DISPLAY_UPDATE_MS        100    // 10 FPS - Watchdog friendly
#define LINE_BUFFER_SIZE         512    // Max line length

/* =============================================================================
 * SCREENSAVER THEME COLORS
 * ========================================================================== */
#define COLOR_SONIC_BG      lv_color_hex(0x00008B)  // Dark Blue
#define COLOR_ART_BG        lv_color_hex(0x8B0000)  // Dark Red
#define COLOR_DK_BG         lv_color_hex(0x5D4037)  // Dark Brown
#define COLOR_PACMAN_BG     lv_color_hex(0x000000)  // Black

/* =============================================================================
 * GLOBAL STATE
 * ========================================================================== */
static pc_stats_t pc_stats;
static SemaphoreHandle_t stats_mutex;
static SemaphoreHandle_t lvgl_mutex;
static volatile uint32_t last_data_ms = 0;

/* Display Handles */
static lvgl_gc9a01_handle_t display_cpu, display_gpu, display_ram, display_network;

/* Screen Handles */
static screen_cpu_t *screen_cpu = NULL;
static screen_gpu_t *screen_gpu = NULL;
static screen_ram_t *screen_ram = NULL;
static screen_network_t *screen_network = NULL;

/* Red Dot Indicators (stale data warning) */
static lv_obj_t *dot_cpu = NULL;
static lv_obj_t *dot_gpu = NULL;
static lv_obj_t *dot_ram = NULL;
static lv_obj_t *dot_net = NULL;

/* Screensaver Objects */
static lv_obj_t *screensaver_cpu = NULL;
static lv_obj_t *screensaver_gpu = NULL;
static lv_obj_t *screensaver_ram = NULL;
static lv_obj_t *screensaver_net = NULL;
static bool is_screensaver_active = false;

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
 * HELPER: Create Red Dot Indicator
 * ========================================================================== */
static lv_obj_t *create_status_dot(lv_obj_t *parent)
{
    if (!parent) return NULL;

    lv_obj_t *dot = lv_obj_create(parent);
    lv_obj_set_size(dot, 12, 12);  // Radius 6 = Diameter 12
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dot, 1, 0);
    lv_obj_set_style_border_color(dot, lv_color_white(), 0);
    lv_obj_align(dot, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);  // Hidden by default

    return dot;
}

/* =============================================================================
 * HELPER: Create Screensaver Overlay
 * ========================================================================== */
static lv_obj_t *create_screensaver(lv_obj_t *parent, lv_color_t bg_color, const char *text)
{
    if (!parent) return NULL;

    // Fullscreen overlay container
    lv_obj_t *overlay = lv_obj_create(parent);
    lv_obj_set_size(overlay, 240, 240);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_style_bg_color(overlay, bg_color, 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_radius(overlay, 0, 0);
    lv_obj_set_style_pad_all(overlay, 0, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

    // Centered text label
    lv_obj_t *label = lv_label_create(overlay);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    // Start hidden
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);

    return overlay;
}

/* =============================================================================
 * HELPER: Show/Hide Screensavers
 * ========================================================================== */
static void show_screensavers(void)
{
    if (screensaver_cpu) lv_obj_clear_flag(screensaver_cpu, LV_OBJ_FLAG_HIDDEN);
    if (screensaver_gpu) lv_obj_clear_flag(screensaver_gpu, LV_OBJ_FLAG_HIDDEN);
    if (screensaver_ram) lv_obj_clear_flag(screensaver_ram, LV_OBJ_FLAG_HIDDEN);
    if (screensaver_net) lv_obj_clear_flag(screensaver_net, LV_OBJ_FLAG_HIDDEN);
}

static void hide_screensavers(void)
{
    if (screensaver_cpu) lv_obj_add_flag(screensaver_cpu, LV_OBJ_FLAG_HIDDEN);
    if (screensaver_gpu) lv_obj_add_flag(screensaver_gpu, LV_OBJ_FLAG_HIDDEN);
    if (screensaver_ram) lv_obj_add_flag(screensaver_ram, LV_OBJ_FLAG_HIDDEN);
    if (screensaver_net) lv_obj_add_flag(screensaver_net, LV_OBJ_FLAG_HIDDEN);
}

/* =============================================================================
 * HELPER: Show/Hide Red Dots
 * ========================================================================== */
static void show_red_dots(void)
{
    if (dot_cpu) lv_obj_clear_flag(dot_cpu, LV_OBJ_FLAG_HIDDEN);
    if (dot_gpu) lv_obj_clear_flag(dot_gpu, LV_OBJ_FLAG_HIDDEN);
    if (dot_ram) lv_obj_clear_flag(dot_ram, LV_OBJ_FLAG_HIDDEN);
    if (dot_net) lv_obj_clear_flag(dot_net, LV_OBJ_FLAG_HIDDEN);
}

static void hide_red_dots(void)
{
    if (dot_cpu) lv_obj_add_flag(dot_cpu, LV_OBJ_FLAG_HIDDEN);
    if (dot_gpu) lv_obj_add_flag(dot_gpu, LV_OBJ_FLAG_HIDDEN);
    if (dot_ram) lv_obj_add_flag(dot_ram, LV_OBJ_FLAG_HIDDEN);
    if (dot_net) lv_obj_add_flag(dot_net, LV_OBJ_FLAG_HIDDEN);
}

/* =============================================================================
 * DATA PARSER - Robust Line Buffer Implementation
 * ========================================================================== */
static void parse_pc_data(const char *line)
{
    if (!line || strlen(line) < 5) return;

    // Make a mutable copy for strtok
    char buffer[LINE_BUFFER_SIZE];
    strncpy(buffer, line, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    xSemaphoreTake(stats_mutex, portMAX_DELAY);

    int fields_parsed = 0;
    char *token = strtok(buffer, ",");

    while (token != NULL) {
        // CPU Usage
        if (strncmp(token, "CPU:", 4) == 0) {
            pc_stats.cpu_percent = (uint8_t)atoi(token + 4);
            fields_parsed++;
        }
        // CPU Temperature
        else if (strncmp(token, "CPUT:", 5) == 0) {
            pc_stats.cpu_temp = atof(token + 5);
            fields_parsed++;
        }
        // GPU Usage
        else if (strncmp(token, "GPU:", 4) == 0) {
            pc_stats.gpu_percent = (uint8_t)atoi(token + 4);
            fields_parsed++;
        }
        // GPU Temperature
        else if (strncmp(token, "GPUT:", 5) == 0) {
            pc_stats.gpu_temp = atof(token + 5);
            fields_parsed++;
        }
        // VRAM
        else if (strncmp(token, "VRAM:", 5) == 0) {
            sscanf(token + 5, "%f/%f", &pc_stats.gpu_vram_used, &pc_stats.gpu_vram_total);
            fields_parsed++;
        }
        // RAM
        else if (strncmp(token, "RAM:", 4) == 0) {
            sscanf(token + 4, "%f/%f", &pc_stats.ram_used_gb, &pc_stats.ram_total_gb);
            if (pc_stats.ram_total_gb < 0.1f) {
                pc_stats.ram_total_gb = 16.0f;
            }
            fields_parsed++;
        }
        // Network Type
        else if (strncmp(token, "NET:", 4) == 0) {
            strncpy(pc_stats.net_type, token + 4, sizeof(pc_stats.net_type) - 1);
            pc_stats.net_type[sizeof(pc_stats.net_type) - 1] = '\0';
            fields_parsed++;
        }
        // Network Speed
        else if (strncmp(token, "SPEED:", 6) == 0) {
            strncpy(pc_stats.net_speed, token + 6, sizeof(pc_stats.net_speed) - 1);
            pc_stats.net_speed[sizeof(pc_stats.net_speed) - 1] = '\0';
            fields_parsed++;
        }
        // Download Speed
        else if (strncmp(token, "DOWN:", 5) == 0) {
            pc_stats.net_down_mbps = atof(token + 5);
            fields_parsed++;
        }
        // Upload Speed
        else if (strncmp(token, "UP:", 3) == 0) {
            pc_stats.net_up_mbps = atof(token + 3);
            fields_parsed++;
        }

        token = strtok(NULL, ",");
    }

    // Update timestamp if we got ANY valid data
    if (fields_parsed > 0) {
        last_data_ms = (uint32_t)(esp_timer_get_time() / 1000);
        ESP_LOGD(TAG, "Parsed %d fields, timestamp updated", fields_parsed);
    }

    xSemaphoreGive(stats_mutex);
}

/* =============================================================================
 * TASK: USB Serial RX - Robust Line Buffer
 * ========================================================================== */
static void usb_rx_task(void *arg)
{
    static uint8_t rx_buf[256];
    static char line_buf[LINE_BUFFER_SIZE];
    static int line_pos = 0;

    ESP_LOGI(TAG, "USB RX Task started");

    while (1) {
        // Read with timeout so other tasks can run
        int len = usb_serial_jtag_read_bytes(rx_buf, sizeof(rx_buf), pdMS_TO_TICKS(10));

        if (len > 0) {
            for (int i = 0; i < len; i++) {
                uint8_t c = rx_buf[i];

                // End of line - process it
                if (c == '\n' || c == '\r') {
                    if (line_pos > 0) {
                        line_buf[line_pos] = '\0';
                        parse_pc_data(line_buf);
                        line_pos = 0;  // Reset for next line
                    }
                }
                // Normal character - add to buffer
                else {
                    if (line_pos < LINE_BUFFER_SIZE - 1) {
                        line_buf[line_pos++] = (char)c;
                    } else {
                        // Buffer overflow - discard and reset
                        ESP_LOGW(TAG, "Line buffer overflow, discarding");
                        line_pos = 0;
                    }
                }
            }

            // Yield to watchdog after processing data
            vTaskDelay(pdMS_TO_TICKS(1));
        } else {
            // No data - longer delay
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

/* =============================================================================
 * TASK: Display Update - 10 FPS with Screensaver Logic
 * ========================================================================== */
static void display_update_task(void *arg)
{
    // Initialize timestamp
    last_data_ms = (uint32_t)(esp_timer_get_time() / 1000);

    ESP_LOGI(TAG, "Display Update Task started (10 FPS)");

    while (1) {
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        uint32_t time_since_data = now - last_data_ms;

        bool data_is_stale = (time_since_data > STALE_DATA_THRESHOLD_MS);
        bool should_screensave = (time_since_data > SCREENSAVER_TIMEOUT_MS);

        // Try to take LVGL mutex (don't block forever)
        if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {

            // =================================================================
            // SCREENSAVER LOGIC
            // =================================================================
            if (should_screensave && !is_screensaver_active) {
                is_screensaver_active = true;
                show_screensavers();
                ESP_LOGW(TAG, "Screensaver ON (no data for %lu ms)", time_since_data);
            }
            else if (!should_screensave && is_screensaver_active) {
                is_screensaver_active = false;
                hide_screensavers();
                ESP_LOGI(TAG, "Screensaver OFF (data received)");
            }

            // =================================================================
            // RED DOT LOGIC
            // =================================================================
            if (data_is_stale && !is_screensaver_active) {
                show_red_dots();
            } else {
                hide_red_dots();
            }

            // =================================================================
            // UPDATE SCREENS (only if not in screensaver)
            // =================================================================
            if (!is_screensaver_active) {
                // Get local copy of stats
                xSemaphoreTake(stats_mutex, portMAX_DELAY);
                pc_stats_t local_stats = pc_stats;
                xSemaphoreGive(stats_mutex);

                // Update all screens
                if (screen_cpu) screen_cpu_update(screen_cpu, &local_stats);
                if (screen_gpu) screen_gpu_update(screen_gpu, &local_stats);
                if (screen_ram) screen_ram_update(screen_ram, &local_stats);
                if (screen_network) screen_network_update(screen_network, &local_stats);
            }

            xSemaphoreGive(lvgl_mutex);
        }

        // DESERT-SPEC: 100ms delay = 10 FPS, Watchdog friendly
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
        if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            uint32_t time_till_next = lv_timer_handler();
            xSemaphoreGive(lvgl_mutex);

            // Use the time LVGL tells us to wait, but cap it
            uint32_t delay_ms = (time_till_next < 5) ? 5 : time_till_next;
            if (delay_ms > 30) delay_ms = 30;
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        } else {
            // Couldn't get mutex, yield to other tasks
            vTaskDelay(pdMS_TO_TICKS(5));
        }

        // Extra yield to let IDLE task run (feed watchdog)
        taskYIELD();
    }
}

/* =============================================================================
 * MAIN APPLICATION ENTRY
 * ========================================================================== */
void app_main(void)
{
    ESP_LOGI(TAG, "===========================================");
    ESP_LOGI(TAG, "PC Monitor - Desert-Spec Phase 2");
    ESP_LOGI(TAG, "===========================================");

    // Create mutexes
    stats_mutex = xSemaphoreCreateMutex();
    lvgl_mutex = xSemaphoreCreateMutex();

    if (!stats_mutex || !lvgl_mutex) {
        ESP_LOGE(TAG, "Failed to create mutexes!");
        return;
    }

    // Initialize USB Serial JTAG
    usb_serial_jtag_driver_config_t usb_cfg = {
        .rx_buffer_size = 2048,
        .tx_buffer_size = 1024
    };
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_cfg));
    ESP_LOGI(TAG, "USB Serial JTAG initialized");

    // Initialize SPI Bus
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

    // Initialize LVGL
    lv_init();
    ESP_LOGI(TAG, "LVGL initialized");

    // Initialize displays and screens (under mutex)
    xSemaphoreTake(lvgl_mutex, portMAX_DELAY);

    // Display 1: CPU
    ESP_LOGI(TAG, "Initializing CPU display...");
    lvgl_gc9a01_init(&config_cpu, &display_cpu);
    screen_cpu = screen_cpu_create(lvgl_gc9a01_get_display(&display_cpu));
    if (screen_cpu && screen_cpu->screen) {
        dot_cpu = create_status_dot(screen_cpu->screen);
        screensaver_cpu = create_screensaver(screen_cpu->screen, COLOR_SONIC_BG, "SONIC");
    }

    // Display 2: GPU
    ESP_LOGI(TAG, "Initializing GPU display...");
    lvgl_gc9a01_init(&config_gpu, &display_gpu);
    screen_gpu = screen_gpu_create(lvgl_gc9a01_get_display(&display_gpu));
    if (screen_gpu && screen_gpu->screen) {
        dot_gpu = create_status_dot(screen_gpu->screen);
        screensaver_gpu = create_screensaver(screen_gpu->screen, COLOR_ART_BG, "ART");
    }

    // Display 3: RAM
    ESP_LOGI(TAG, "Initializing RAM display...");
    lvgl_gc9a01_init(&config_ram, &display_ram);
    screen_ram = screen_ram_create(lvgl_gc9a01_get_display(&display_ram));
    if (screen_ram && screen_ram->screen) {
        dot_ram = create_status_dot(screen_ram->screen);
        screensaver_ram = create_screensaver(screen_ram->screen, COLOR_DK_BG, "DONKEY KONG");
    }

    // Display 4: Network
    ESP_LOGI(TAG, "Initializing Network display...");
    lvgl_gc9a01_init(&config_net, &display_network);
    screen_network = screen_network_create(lvgl_gc9a01_get_display(&display_network));
    if (screen_network && screen_network->screen) {
        dot_net = create_status_dot(screen_network->screen);
        screensaver_net = create_screensaver(screen_network->screen, COLOR_PACMAN_BG, "PAC-MAN");
    }

    xSemaphoreGive(lvgl_mutex);
    ESP_LOGI(TAG, "All displays initialized");

    // Create tasks with WATCHDOG-FRIENDLY priorities
    // All tasks at low priority so IDLE can run and feed watchdog

    // USB RX: Priority 3
    xTaskCreate(usb_rx_task, "usb_rx", 4096, NULL, 3, NULL);

    // LVGL Tick: Priority 2 (just needs to run periodically)
    xTaskCreate(lvgl_tick_task, "lv_tick", 2048, NULL, 2, NULL);

    // LVGL Timer: Priority 2, pinned to Core 1
    // Lower priority = IDLE1 can run and feed watchdog
    xTaskCreatePinnedToCore(lvgl_timer_task, "lv_timer", 8192, NULL, 2, NULL, 1);

    // Display Update: Priority 2, pinned to Core 0
    xTaskCreatePinnedToCore(display_update_task, "update", 4096, NULL, 2, NULL, 0);

    ESP_LOGI(TAG, "===========================================");
    ESP_LOGI(TAG, "System ready. Waiting for USB data...");
    ESP_LOGI(TAG, "Screensaver in %d seconds if no data", SCREENSAVER_TIMEOUT_MS / 1000);
    ESP_LOGI(TAG, "===========================================");
}
