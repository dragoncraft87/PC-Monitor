/**
 * @file main_lvgl.c
 * @brief PC Monitor - Desert-Spec Edition (Watchdog Fix)
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
#include "screens/screen_cpu_lvgl.c"
#include "screens/screen_gpu_lvgl.c"
#include "screens/screen_ram_lvgl.c"
#include "screens/screen_network_lvgl.c"

static const char *TAG = "PC-MONITOR";

/* ================= CONFIGURATION ================= */
#define SCREENSAVER_TIMEOUT_MS 5000
#define STALE_DATA_THRESHOLD_MS 1000 

/* Global State */
static pc_stats_t pc_stats;
static SemaphoreHandle_t stats_mutex;
static SemaphoreHandle_t lvgl_mutex;
static volatile uint32_t last_data_ms = 0;

/* Displays & Screens */
static lvgl_gc9a01_handle_t display_cpu, display_gpu, display_ram, display_network;
static screen_cpu_t *screen_cpu = NULL;
static screen_gpu_t *screen_gpu = NULL;
static screen_ram_t *screen_ram = NULL;
static screen_network_t *screen_network = NULL;

/* Red Dots */
static lv_obj_t *dot_cpu = NULL;
static lv_obj_t *dot_gpu = NULL;
static lv_obj_t *dot_ram = NULL;
static lv_obj_t *dot_net = NULL;

static bool is_screensaver = false;

/* SPI Configs */
static const lvgl_gc9a01_config_t config_cpu = { .pin_sck=4, .pin_mosi=5, .pin_cs=12, .pin_dc=11, .pin_rst=13, .spi_host=SPI2_HOST };
static const lvgl_gc9a01_config_t config_gpu = { .pin_sck=4, .pin_mosi=5, .pin_cs=9,  .pin_dc=46, .pin_rst=10, .spi_host=SPI2_HOST };
static const lvgl_gc9a01_config_t config_ram = { .pin_sck=4, .pin_mosi=5, .pin_cs=8,  .pin_dc=18, .pin_rst=3,  .spi_host=SPI2_HOST };
static const lvgl_gc9a01_config_t config_net = { .pin_sck=4, .pin_mosi=5, .pin_cs=16, .pin_dc=15, .pin_rst=17, .spi_host=SPI2_HOST };

/* Helper: Red Dot */
static lv_obj_t* create_status_dot(lv_obj_t *parent) {
    if (!parent) return NULL;
    lv_obj_t *dot = lv_obj_create(parent);
    lv_obj_set_size(dot, 12, 12);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_style_border_width(dot, 1, 0);
    lv_obj_set_style_border_color(dot, lv_color_white(), 0);
    lv_obj_align(dot, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN); 
    return dot;
}

/* Parser */
void parse_pc_data(char *line) {
    xSemaphoreTake(stats_mutex, portMAX_DELAY);
    
    char *token = strtok(line, ",");
    int fields_found = 0;

    while (token != NULL) {
        if (strncmp(token, "CPU:", 4) == 0) { pc_stats.cpu_percent = atoi(token + 4); fields_found++; }
        else if (strncmp(token, "CPUT:", 5) == 0) pc_stats.cpu_temp = atof(token + 5);
        else if (strncmp(token, "GPU:", 4) == 0) pc_stats.gpu_percent = atoi(token + 4);
        else if (strncmp(token, "GPUT:", 5) == 0) pc_stats.gpu_temp = atof(token + 5);
        else if (strncmp(token, "VRAM:", 5) == 0) sscanf(token + 5, "%f/%f", &pc_stats.gpu_vram_used, &pc_stats.gpu_vram_total);
        else if (strncmp(token, "RAM:", 4) == 0) {
            sscanf(token + 4, "%f/%f", &pc_stats.ram_used_gb, &pc_stats.ram_total_gb);
            if (pc_stats.ram_total_gb < 0.1f) pc_stats.ram_total_gb = 16.0f; 
        }
        else if (strncmp(token, "NET:", 4) == 0) {
            strncpy(pc_stats.net_type, token + 4, sizeof(pc_stats.net_type) - 1);
            pc_stats.net_type[sizeof(pc_stats.net_type) - 1] = '\0';
        }
        else if (strncmp(token, "SPEED:", 6) == 0) {
            strncpy(pc_stats.net_speed, token + 6, sizeof(pc_stats.net_speed) - 1);
            pc_stats.net_speed[sizeof(pc_stats.net_speed) - 1] = '\0';
        }
        else if (strncmp(token, "DOWN:", 5) == 0) pc_stats.net_down_mbps = atof(token + 5);
        else if (strncmp(token, "UP:", 3) == 0) pc_stats.net_up_mbps = atof(token + 3);
        
        token = strtok(NULL, ",");
    }

    if (fields_found > 0) {
        last_data_ms = (uint32_t)(esp_timer_get_time() / 1000);
    }
    xSemaphoreGive(stats_mutex);
}

/* TASKS */
void usb_rx_task(void *arg) {
    static uint8_t rx_buf[1024];
    static uint8_t line_buf[1024];
    static int line_pos = 0;

    ESP_LOGI(TAG, "USB RX Task started");

    while (1) {
        // Lese mit Timeout, damit andere Tasks auch mal drankommen
        int len = usb_serial_jtag_read_bytes(rx_buf, sizeof(rx_buf), pdMS_TO_TICKS(10));
        
        if (len > 0) {
            for (int i = 0; i < len; i++) {
                uint8_t c = rx_buf[i];
                if (c == '\n' || c == '\r') {
                    if (line_pos > 0) {
                        line_buf[line_pos] = 0; 
                        parse_pc_data((char*)line_buf);
                        line_pos = 0; 
                    }
                } else {
                    if (line_pos < sizeof(line_buf) - 1) line_buf[line_pos++] = c;
                    else line_pos = 0; // Overflow
                }
            }
            // CRITICAL FIX: Gib dem Watchdog eine Chance!
            vTaskDelay(pdMS_TO_TICKS(1)); 
        } else {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

void display_update_task(void *arg) {
    last_data_ms = (uint32_t)(esp_timer_get_time() / 1000);

    while (1) {
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        uint32_t diff = now - last_data_ms;
        bool stale_data = (diff > STALE_DATA_THRESHOLD_MS);
        bool should_screensaver = (diff > SCREENSAVER_TIMEOUT_MS);

        if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(100)) == pdTRUE) { // Try Take
            
            /* Screensaver Logic */
            if (should_screensaver && !is_screensaver) {
                is_screensaver = true;
                ESP_LOGW(TAG, "Screensaver ON");
                // TODO: Show Images
            } else if (!should_screensaver && is_screensaver) {
                is_screensaver = false;
                ESP_LOGI(TAG, "Screensaver OFF");
            }

            /* Red Dots (Lazy Init) */
            if (!dot_cpu && screen_cpu) dot_cpu = create_status_dot(screen_cpu->screen);
            if (!dot_gpu && screen_gpu) dot_gpu = create_status_dot(screen_gpu->screen);
            if (!dot_ram && screen_ram) dot_ram = create_status_dot(screen_ram->screen);
            if (!dot_net && screen_network) dot_net = create_status_dot(screen_network->screen);

            if (dot_cpu) { if (stale_data) lv_obj_clear_flag(dot_cpu, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(dot_cpu, LV_OBJ_FLAG_HIDDEN); }
            if (dot_gpu) { if (stale_data) lv_obj_clear_flag(dot_gpu, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(dot_gpu, LV_OBJ_FLAG_HIDDEN); }
            if (dot_ram) { if (stale_data) lv_obj_clear_flag(dot_ram, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(dot_ram, LV_OBJ_FLAG_HIDDEN); }
            if (dot_net) { if (stale_data) lv_obj_clear_flag(dot_net, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(dot_net, LV_OBJ_FLAG_HIDDEN); }

            /* Update Stats */
            xSemaphoreTake(stats_mutex, portMAX_DELAY);
            pc_stats_t local = pc_stats;
            xSemaphoreGive(stats_mutex);

            if (screen_cpu) screen_cpu_update(screen_cpu, &local);
            if (screen_gpu) screen_gpu_update(screen_gpu, &local);
            if (screen_ram) screen_ram_update(screen_ram, &local);
            if (screen_network) screen_network_update(screen_network, &local);

            xSemaphoreGive(lvgl_mutex);
        }
        
        vTaskDelay(pdMS_TO_TICKS(200)); 
    }
}

// FIX: Erhöhtes Delay für Tick Task, damit IDLE Task laufen kann
void lvgl_tick_task(void *arg) {
    while(1) { 
        lv_tick_inc(10); 
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}

void lvgl_timer_task(void *arg) {
    while(1) {
        if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            lv_timer_handler();
            xSemaphoreGive(lvgl_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void) {
    stats_mutex = xSemaphoreCreateMutex();
    lvgl_mutex = xSemaphoreCreateMutex();
    
    usb_serial_jtag_driver_config_t usb_cfg = { .rx_buffer_size = 2048, .tx_buffer_size = 1024 };
    usb_serial_jtag_driver_install(&usb_cfg);

    spi_bus_config_t buscfg = {
        .mosi_io_num = 5, .sclk_io_num = 4, .miso_io_num = -1, 
        .quadwp_io_num = -1, .quadhd_io_num = -1, 
        .max_transfer_sz = 240*240*2*2
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    lv_init();
    
    xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
    
    lvgl_gc9a01_init(&config_cpu, &display_cpu);
    screen_cpu = screen_cpu_create(lvgl_gc9a01_get_display(&display_cpu));

    lvgl_gc9a01_init(&config_gpu, &display_gpu);
    screen_gpu = screen_gpu_create(lvgl_gc9a01_get_display(&display_gpu));

    lvgl_gc9a01_init(&config_ram, &display_ram);
    screen_ram = screen_ram_create(lvgl_gc9a01_get_display(&display_ram));

    lvgl_gc9a01_init(&config_net, &display_network);
    screen_network = screen_network_create(lvgl_gc9a01_get_display(&display_network));
    
    xSemaphoreGive(lvgl_mutex);

    // FIX: Angepasste Prioritäten
    xTaskCreate(usb_rx_task, "usb_rx", 4096, NULL, 5, NULL); // Prio 5 (niedrig)
    xTaskCreate(lvgl_tick_task, "lv_tick", 2048, NULL, 10, NULL); // Prio 10
    xTaskCreatePinnedToCore(lvgl_timer_task, "lv_timer", 8192, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(display_update_task, "update", 4096, NULL, 5, NULL, 0);
    
    ESP_LOGI(TAG, "System initialized. Waiting for USB Data...");
}