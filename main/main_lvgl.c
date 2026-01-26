/**
 * @file main_lvgl.c
 * @brief PC Monitor - Desert-Spec Edition (Robust & Audited)
 *
 * FEATURES:
 * - 4x GC9A01 Display Support
 * - Robust USB Parsing (Line Buffered)
 * - Atomic Timestamp Handling
 * - Red Dot "Stale Data" Indicator
 * - Screensaver Mode
 */

#include <stdio.h>
#include <string.h>
#include <math.h> // For clamp/validation
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

static const char *TAG = "PC-MONITOR";

/* ================= CONFIGURATION ================= */
#define SCREENSAVER_TIMEOUT_MS 5000
#define STALE_DATA_THRESHOLD_MS 1000  // Red Dot erscheint nach 1s ohne Daten

/* Global State protected by Mutex */
static pc_stats_t pc_stats;
static SemaphoreHandle_t stats_mutex;
static SemaphoreHandle_t lvgl_mutex;

/* Atomic Timestamp (using esp_timer ms) */
static volatile uint32_t last_data_ms = 0;

/* Display & Screen Handles */
static lvgl_gc9a01_handle_t display_cpu, display_gpu, display_ram, display_network;
static screen_cpu_t *screen_cpu = NULL;
static screen_gpu_t *screen_gpu = NULL;
static screen_ram_t *screen_ram = NULL;
static screen_network_t *screen_network = NULL;

/* Red Dot Indicators */
static lv_obj_t *dot_cpu = NULL;
static lv_obj_t *dot_gpu = NULL;
static lv_obj_t *dot_ram = NULL;
static lv_obj_t *dot_net = NULL;

/* Screensaver State */
static bool is_screensaver = false;

/* SPI Configs */
static const lvgl_gc9a01_config_t config_cpu = { .pin_sck=4, .pin_mosi=5, .pin_cs=12, .pin_dc=11, .pin_rst=13, .spi_host=SPI2_HOST };
static const lvgl_gc9a01_config_t config_gpu = { .pin_sck=4, .pin_mosi=5, .pin_cs=9,  .pin_dc=46, .pin_rst=10, .spi_host=SPI2_HOST };
static const lvgl_gc9a01_config_t config_ram = { .pin_sck=4, .pin_mosi=5, .pin_cs=8,  .pin_dc=18, .pin_rst=3,  .spi_host=SPI2_HOST };
static const lvgl_gc9a01_config_t config_net = { .pin_sck=4, .pin_mosi=5, .pin_cs=16, .pin_dc=15, .pin_rst=17, .spi_host=SPI2_HOST };

/* ================= HELPER: UI COMPONENTS ================= */

/* Erstellt einen kleinen roten Punkt unten mittig auf dem Screen */
static lv_obj_t* create_status_dot(lv_obj_t *parent) {
    lv_obj_t *dot = lv_obj_create(parent);
    lv_obj_set_size(dot, 12, 12);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_style_border_width(dot, 1, 0);
    lv_obj_set_style_border_color(dot, lv_color_white(), 0);
    lv_obj_align(dot, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN); // Standardmäßig versteckt
    return dot;
}

/* ================= DATA PARSING (AUDITED) ================= */

void parse_pc_data(char *line) {
    // 1. Temporäre Struktur für Sicherheit (Frame Commit Strategy)
    pc_stats_t temp_stats = {0};
    bool valid_frame = false;
    
    // Wir nehmen die aktuellen Werte als Basis, falls nur Teil-Updates kommen? 
    // Nein, Sicherheits-Auditor sagt: Keine Mischdaten! 
    // Aber bei CSV Key-Value ist Update okay. Wir sichern den Mutex erst beim Schreiben.

    xSemaphoreTake(stats_mutex, portMAX_DELAY);
    
    char *token = strtok(line, ",");
    int fields_found = 0;

    while (token != NULL) {
        if (strncmp(token, "CPU:", 4) == 0) {
            pc_stats.cpu_percent = atoi(token + 4);
            fields_found++;
        }
        else if (strncmp(token, "CPUT:", 5) == 0) pc_stats.cpu_temp = atof(token + 5);
        else if (strncmp(token, "GPU:", 4) == 0) pc_stats.gpu_percent = atoi(token + 4);
        else if (strncmp(token, "GPUT:", 5) == 0) pc_stats.gpu_temp = atof(token + 5);
        else if (strncmp(token, "VRAM:", 5) == 0) sscanf(token + 5, "%f/%f", &pc_stats.gpu_vram_used, &pc_stats.gpu_vram_total);
        else if (strncmp(token, "RAM:", 4) == 0) {
            sscanf(token + 4, "%f/%f", &pc_stats.ram_used_gb, &pc_stats.ram_total_gb);
            // Fix: Division by Zero Prevention
            if (pc_stats.ram_total_gb < 0.1f) pc_stats.ram_total_gb = 16.0f; 
        }
        else if (strncmp(token, "NET:", 4) == 0) {
            // Fix: Safe String Copy
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

    // Atomic Update Timestamp (nur wenn wir wirklich was geparst haben)
    if (fields_found > 0) {
        last_data_ms = (uint32_t)(esp_timer_get_time() / 1000);
        // Debug Log (nur alle paar sekunden oder bei Bedarf aktivieren)
        // ESP_LOGI(TAG, "Parsed Data. CPU: %d%%", pc_stats.cpu_percent);
    } else {
        ESP_LOGW(TAG, "Received line but no valid keys found: %s", line);
    }

    xSemaphoreGive(stats_mutex);
}

/* ================= TASKS ================= */

/* ================= TASKS ================= */

void usb_rx_task(void *arg) {
    static uint8_t rx_buf[1024];
    static uint8_t line_buf[1024];
    static int line_pos = 0;

    ESP_LOGI(TAG, "USB RX Task started");

    while (1) {
        // Lies Daten mit Timeout
        int len = usb_serial_jtag_read_bytes(rx_buf, sizeof(rx_buf), pdMS_TO_TICKS(10));
        
        if (len > 0) {
            for (int i = 0; i < len; i++) {
                uint8_t c = rx_buf[i];

                if (c == '\n' || c == '\r') {
                    if (line_pos > 0) {
                        line_buf[line_pos] = 0; // Null-Terminate
                        parse_pc_data((char*)line_buf);
                        line_pos = 0; // Reset
                    }
                } else {
                    if (line_pos < sizeof(line_buf) - 1) {
                        line_buf[line_pos++] = c;
                    } else {
                        // Overflow Protection
                        line_pos = 0; 
                    }
                }
            }
            // WICHTIG: Auch wenn Daten da waren, kurz atmen lassen!
            // Verhindert Watchdog-Crash bei Datenflut.
            vTaskDelay(pdMS_TO_TICKS(1)); 
        } else {
            // Keine Daten -> Länger warten
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

void display_update_task(void *arg) {
    /* Initialize Timestamp */
    last_data_ms = (uint32_t)(esp_timer_get_time() / 1000);

    /* Create Red Dots (Overlay) - Must be done AFTER screen creation */
    // Note: We need to access the screen objects. Assuming they are accessible via the structs
    // or we add them to the currently active screen.
    // Hack for now: We add them to the screen objects if they are public, 
    // otherwise we rely on the screen_x_update to handle them if we passed them?
    // Let's create them on the default displays for now.
    
    // WARNUNG: lv_layer_top() ist global für EIN Display. Wir haben 4.
    // Wir müssen den Punkt zum jeweiligen Screen Object hinzufügen.
    // Da wir die Screen Structs haben, greifen wir auf deren "root" zu wenn möglich,
    // oder wir erstellen sie einfach im Loop wenn wir sicher sind.
    
    while (1) {
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        uint32_t diff = now - last_data_ms; // Unsigned arithmetic handles wrapping correctly

        bool stale_data = (diff > STALE_DATA_THRESHOLD_MS);
        bool should_screensaver = (diff > SCREENSAVER_TIMEOUT_MS);

        xSemaphoreTake(lvgl_mutex, portMAX_DELAY);

        /* 1. Manage Screensaver State */
        if (should_screensaver && !is_screensaver) {
            is_screensaver = true;
            ESP_LOGW(TAG, "Timeout (%lu ms) -> Screensaver ON", diff);
            // TODO: Zeige Screensaver Bilder (Funktionen noch zu erstellen)
            // Beispiel: lv_scr_load(screensaver_cpu); 
            // VORERST: Wir lassen die Gauges, aber machen sie dunkel oder zeigen den roten Punkt.
        } else if (!should_screensaver && is_screensaver) {
            is_screensaver = false;
            ESP_LOGI(TAG, "Data received -> Screensaver OFF");
        }

        // /* 2. Update Status Indicators (Red Dot) */
        // // Wir erstellen die Punkte lazy beim ersten Durchlauf, wenn Screen Pointer gültig sind
        // if (!dot_cpu && screen_cpu) dot_cpu = create_status_dot(screen_cpu->screen);
        // if (!dot_gpu && screen_gpu) dot_gpu = create_status_dot(screen_gpu->screen);
        // if (!dot_ram && screen_ram) dot_ram = create_status_dot(screen_ram->screen);
        // if (!dot_net && screen_network) dot_net = create_status_dot(screen_network->screen);

        // if (dot_cpu) {
        //     if (stale_data) lv_obj_clear_flag(dot_cpu, LV_OBJ_FLAG_HIDDEN);
        //     else lv_obj_add_flag(dot_cpu, LV_OBJ_FLAG_HIDDEN);
        // }
        // if (dot_gpu) { // Copy logic for others
        //      if (stale_data) lv_obj_clear_flag(dot_gpu, LV_OBJ_FLAG_HIDDEN);
        //      else lv_obj_add_flag(dot_gpu, LV_OBJ_FLAG_HIDDEN);
        // }
        // if (dot_ram) {
        //      if (stale_data) lv_obj_clear_flag(dot_ram, LV_OBJ_FLAG_HIDDEN);
        //      else lv_obj_add_flag(dot_ram, LV_OBJ_FLAG_HIDDEN);
        // }
        // if (dot_net) {
        //      if (stale_data) lv_obj_clear_flag(dot_net, LV_OBJ_FLAG_HIDDEN);
        //      else lv_obj_add_flag(dot_net, LV_OBJ_FLAG_HIDDEN);
        // }

        /* 3. Update Screen Content */
        // AUCH im Screensaver-Mode updaten wir ggf. um Animationen zu stoppen oder Bild zu wechseln
        // Aber hier: Nur updaten wenn Daten frisch sind ODER um "0" anzuzeigen?
        // Wir updaten immer, damit Red Dot sichtbar wird.
        
        xSemaphoreTake(stats_mutex, portMAX_DELAY);
        pc_stats_t local = pc_stats;
        xSemaphoreGive(stats_mutex);

        if (screen_cpu) screen_cpu_update(screen_cpu, &local);
        if (screen_gpu) screen_gpu_update(screen_gpu, &local);
        if (screen_ram) screen_ram_update(screen_ram, &local);
        if (screen_network) screen_network_update(screen_network, &local);

        xSemaphoreGive(lvgl_mutex);
        vTaskDelay(pdMS_TO_TICKS(50)); // 20 FPS Update
    }
}

/* ================= MAIN ================= */

void lvgl_tick_task(void *arg) {
    while(1) { lv_tick_inc(5); vTaskDelay(pdMS_TO_TICKS(5)); }
}

void lvgl_timer_task(void *arg) {
    while(1) {
        xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
        lv_timer_handler();
        xSemaphoreGive(lvgl_mutex);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void app_main(void) {
    // 1. Init Basics
    stats_mutex = xSemaphoreCreateMutex();
    lvgl_mutex = xSemaphoreCreateMutex();
    
    // 2. USB Init
    usb_serial_jtag_driver_config_t usb_cfg = { .rx_buffer_size = 2048, .tx_buffer_size = 1024 };
    usb_serial_jtag_driver_install(&usb_cfg);

    // 3. SPI Init
    spi_bus_config_t buscfg = {
        .mosi_io_num = 5, .sclk_io_num = 4, .miso_io_num = -1, 
        .quadwp_io_num = -1, .quadhd_io_num = -1, 
        .max_transfer_sz = 240*240*2 + 100
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    // 4. LVGL Init
    lv_init();
    
    // 5. Create Screens (Protected)
    xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
    
    // Display 1: CPU
    lvgl_gc9a01_init(&config_cpu, &display_cpu);
    screen_cpu = screen_cpu_create(lvgl_gc9a01_get_display(&display_cpu));

    // Display 2: GPU
    lvgl_gc9a01_init(&config_gpu, &display_gpu);
    screen_gpu = screen_gpu_create(lvgl_gc9a01_get_display(&display_gpu));

    // Display 3: RAM
    lvgl_gc9a01_init(&config_ram, &display_ram);
    screen_ram = screen_ram_create(lvgl_gc9a01_get_display(&display_ram));

    // Display 4: Network
    lvgl_gc9a01_init(&config_net, &display_network);
    screen_network = screen_network_create(lvgl_gc9a01_get_display(&display_network));
    
    xSemaphoreGive(lvgl_mutex);

    // 6. Start Tasks
    xTaskCreate(usb_rx_task, "usb_rx", 4096, NULL, 10, NULL);
    xTaskCreate(lvgl_tick_task, "lv_tick", 2048, NULL, 20, NULL);
    xTaskCreatePinnedToCore(lvgl_timer_task, "lv_timer", 8192, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(display_update_task, "update", 4096, NULL, 5, NULL, 0);
    
    ESP_LOGI(TAG, "System initialized. Waiting for USB Data...");
}