/**
 * @file main_lvgl.c
 * @brief PC Monitor - 4x GC9A01 Displays with LVGL (Desert-Spec Stability)
 *
 * FIXED: Updates ALL 4 screens, cleaner USB parsing, Screensaver logic
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/usb_serial_jtag.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"

#include "lvgl.h"
#include "lvgl_gc9a01_driver.h"
#include "screens/screens_lvgl.h"

static const char *TAG = "PC-MONITOR";

/* ============================================================================
 * CUSTOM MEMORY ALLOCATOR - PREFER PSRAM
 * ========================================================================== */
void *lv_custom_malloc(size_t size) {
    if (size == 0) return NULL;
    void *ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (ptr) return ptr;
    ptr = heap_caps_malloc(size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (ptr) return ptr;
    return heap_caps_malloc(size, MALLOC_CAP_INTERNAL);
}

void lv_custom_free(void *ptr) {
    if (ptr) heap_caps_free(ptr);
}

void *lv_custom_realloc(void *ptr, size_t size) {
    if (size == 0) { lv_custom_free(ptr); return NULL; }
    if (!ptr) return lv_custom_malloc(size);
    void *new_ptr = heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM);
    if (new_ptr) return new_ptr;
    new_ptr = lv_custom_malloc(size);
    if (new_ptr) { memcpy(new_ptr, ptr, size); lv_custom_free(ptr); }
    return new_ptr;
}

/* ============================================================================
 * CONFIGURATION & GLOBALS
 * ========================================================================== */
#define SCREENSAVER_TIMEOUT_MS 5000  /* 5 Sekunden Timeout */

/* Global State */
static bool screensaver_active = false;
static int64_t last_data_time = 0;

/* Display Handles */
static lvgl_gc9a01_handle_t display_cpu, display_gpu, display_ram, display_network;

/* Screen Handles */
static screen_cpu_t *screen_cpu = NULL;
static screen_gpu_t *screen_gpu = NULL;
static screen_ram_t *screen_ram = NULL;
static screen_network_t *screen_network = NULL;

/* Screensaver Screen Objects */
static lv_obj_t *ss_screen_cpu = NULL;
static lv_obj_t *ss_screen_gpu = NULL;
static lv_obj_t *ss_screen_ram = NULL;
static lv_obj_t *ss_screen_net = NULL;

/* Stats & Synchronization */
static pc_stats_t pc_stats = {
    .cpu_percent = 0, .cpu_temp = 0,
    .gpu_percent = 0, .gpu_temp = 0, .gpu_vram_used = 0, .gpu_vram_total = 12.0,
    .ram_used_gb = 0, .ram_total_gb = 64.0,
    .net_type = "---", .net_speed = "---", .net_down_mbps = 0, .net_up_mbps = 0
};
static SemaphoreHandle_t stats_mutex;
static SemaphoreHandle_t lvgl_mutex;

/* USB Buffer */
#define USB_RX_BUF_SIZE 1024
static uint8_t usb_line_buffer[USB_RX_BUF_SIZE];
static int usb_line_pos = 0;

/* SPI Configurations */
static const lvgl_gc9a01_config_t config_cpu = { .pin_sck=4, .pin_mosi=5, .pin_cs=12, .pin_dc=11, .pin_rst=13, .spi_host=SPI2_HOST };
static const lvgl_gc9a01_config_t config_gpu = { .pin_sck=4, .pin_mosi=5, .pin_cs=9,  .pin_dc=46, .pin_rst=10, .spi_host=SPI2_HOST };
static const lvgl_gc9a01_config_t config_ram = { .pin_sck=4, .pin_mosi=5, .pin_cs=8,  .pin_dc=18, .pin_rst=3,  .spi_host=SPI2_HOST };
static const lvgl_gc9a01_config_t config_net = { .pin_sck=4, .pin_mosi=5, .pin_cs=16, .pin_dc=15, .pin_rst=17, .spi_host=SPI2_HOST };

/* ============================================================================
 * HELPER: PARSE DATA
 * ========================================================================== */
static void parse_pc_data(char *line) {
    xSemaphoreTake(stats_mutex, portMAX_DELAY);

    last_data_time = esp_timer_get_time() / 1000; // Reset watchdog

    char *token = strtok(line, ",");
    while (token != NULL) {
        if (strncmp(token, "CPU:", 4) == 0) pc_stats.cpu_percent = atoi(token + 4);
        else if (strncmp(token, "CPUT:", 5) == 0) pc_stats.cpu_temp = atof(token + 5);
        else if (strncmp(token, "GPU:", 4) == 0) pc_stats.gpu_percent = atoi(token + 4);
        else if (strncmp(token, "GPUT:", 5) == 0) pc_stats.gpu_temp = atof(token + 5);
        else if (strncmp(token, "VRAM:", 5) == 0) sscanf(token + 5, "%f/%f", &pc_stats.gpu_vram_used, &pc_stats.gpu_vram_total);
        else if (strncmp(token, "RAM:", 4) == 0) sscanf(token + 4, "%f/%f", &pc_stats.ram_used_gb, &pc_stats.ram_total_gb);
        else if (strncmp(token, "NET:", 4) == 0) strncpy(pc_stats.net_type, token + 4, 15);
        else if (strncmp(token, "SPEED:", 6) == 0) strncpy(pc_stats.net_speed, token + 6, 15);
        else if (strncmp(token, "DOWN:", 5) == 0) pc_stats.net_down_mbps = atof(token + 5);
        else if (strncmp(token, "UP:", 3) == 0) pc_stats.net_up_mbps = atof(token + 3);

        token = strtok(NULL, ",");
    }
    xSemaphoreGive(stats_mutex);
}

/* ============================================================================
 * TASK: USB RECEIVER (ROBUST LINE BUFFER)
 * ========================================================================== */
static void usb_rx_task(void *arg) {
    ESP_LOGI(TAG, "USB RX Task started");
    uint8_t chunk[64];

    while (1) {
        int len = usb_serial_jtag_read_bytes(chunk, sizeof(chunk), pdMS_TO_TICKS(20));
        if (len > 0) {
            for (int i = 0; i < len; i++) {
                if (chunk[i] == '\n' || chunk[i] == '\r') {
                    if (usb_line_pos > 0) {
                        usb_line_buffer[usb_line_pos] = 0;
                        parse_pc_data((char*)usb_line_buffer);
                        usb_line_pos = 0;
                    }
                } else {
                    if (usb_line_pos < USB_RX_BUF_SIZE - 1) {
                        usb_line_buffer[usb_line_pos++] = chunk[i];
                    } else {
                        usb_line_pos = 0; // Overflow protection
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ============================================================================
 * SCREENSAVER: CREATE & CONTROL
 * ========================================================================== */

/* Helper: Create a screensaver screen for a display */
static lv_obj_t *create_screensaver_screen(lv_display_t *disp, const char *text, uint32_t color) {
    lv_display_t *old = lv_display_get_default();
    lv_display_set_default(disp);

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);

    /* Theme Label */
    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_center(label);

    if (old) lv_display_set_default(old);
    return scr;
}

/* Helper: Get original screen from screen handle */
extern lv_obj_t *screen_cpu_get_screen(screen_cpu_t *s);
extern lv_obj_t *screen_gpu_get_screen(screen_gpu_t *s);
extern lv_obj_t *screen_ram_get_screen(screen_ram_t *s);
extern lv_obj_t *screen_network_get_screen(screen_network_t *s);

/* Helper: Load screen on specific display (LVGL 9 compatible) */
static void load_screen_on_display(lv_display_t *disp, lv_obj_t *scr) {
    if (!disp || !scr) return;
    lv_display_t *old = lv_display_get_default();
    lv_display_set_default(disp);
    lv_screen_load(scr);
    if (old) lv_display_set_default(old);
}

static void screensaver_activate(void) {
    if (screensaver_active) return;
    screensaver_active = true;
    ESP_LOGW(TAG, "SCREENSAVER ON (no data for %d ms)", SCREENSAVER_TIMEOUT_MS);

    /* Create screensaver screens if not exist */
    if (!ss_screen_cpu) {
        ss_screen_cpu = create_screensaver_screen(lvgl_gc9a01_get_display(&display_cpu), "SONIC", 0x0066FF);
    }
    if (!ss_screen_gpu) {
        ss_screen_gpu = create_screensaver_screen(lvgl_gc9a01_get_display(&display_gpu), "PAINTER", 0xFF6600);
    }
    if (!ss_screen_ram) {
        ss_screen_ram = create_screensaver_screen(lvgl_gc9a01_get_display(&display_ram), "DK", 0xFFCC00);
    }
    if (!ss_screen_net) {
        ss_screen_net = create_screensaver_screen(lvgl_gc9a01_get_display(&display_network), "PAC-MAN", 0xFFFF00);
    }

    /* Load screensaver screens on their respective displays */
    load_screen_on_display(lvgl_gc9a01_get_display(&display_cpu), ss_screen_cpu);
    load_screen_on_display(lvgl_gc9a01_get_display(&display_gpu), ss_screen_gpu);
    load_screen_on_display(lvgl_gc9a01_get_display(&display_ram), ss_screen_ram);
    load_screen_on_display(lvgl_gc9a01_get_display(&display_network), ss_screen_net);

    ESP_LOGI(TAG, "All 4 screensavers loaded");
}

static void screensaver_deactivate(void) {
    if (!screensaver_active) return;
    screensaver_active = false;
    ESP_LOGI(TAG, "SCREENSAVER OFF (data received)");

    /* Restore original screens using lv_disp_load_scr() */
    /* Diese Funktion lädt den Screen auf das Display, zu dem er gehört */
    if (screen_cpu) {
        lv_obj_t *scr = screen_cpu_get_screen(screen_cpu);
        if (scr) lv_disp_load_scr(scr);
    }
    if (screen_gpu) {
        lv_obj_t *scr = screen_gpu_get_screen(screen_gpu);
        if (scr) lv_disp_load_scr(scr);
    }
    if (screen_ram) {
        lv_obj_t *scr = screen_ram_get_screen(screen_ram);
        if (scr) lv_disp_load_scr(scr);
    }
    if (screen_network) {
        lv_obj_t *scr = screen_network_get_screen(screen_network);
        if (scr) lv_disp_load_scr(scr);
    }

    ESP_LOGI(TAG, "All 4 original screens restored");
}

/* ============================================================================
 * TASK: DISPLAY UPDATE & WATCHDOG
 * ========================================================================== */
static void display_update_task(void *arg) {
    ESP_LOGI(TAG, "Display Update Task started");
    last_data_time = esp_timer_get_time() / 1000;
    int log_counter = 0;

    while (1) {
        int64_t now = esp_timer_get_time() / 1000;
        bool timeout = (now - last_data_time) > SCREENSAVER_TIMEOUT_MS;

        xSemaphoreTake(lvgl_mutex, portMAX_DELAY);

        if (timeout) {
            screensaver_activate();
        } else {
            screensaver_deactivate();

            /* Copy stats locally */
            xSemaphoreTake(stats_mutex, portMAX_DELAY);
            pc_stats_t local = pc_stats;
            xSemaphoreGive(stats_mutex);

            /* Update ALL 4 screens */
            if (screen_cpu) screen_cpu_update(screen_cpu, &local);
            if (screen_gpu) screen_gpu_update(screen_gpu, &local);
            if (screen_ram) screen_ram_update(screen_ram, &local);
            if (screen_network) screen_network_update(screen_network, &local);

            /* Debug log every 10 seconds */
            if (++log_counter >= 100) {
                ESP_LOGI(TAG, "CPU:%d%% GPU:%d%% RAM:%.1f/%.1fGB",
                         local.cpu_percent, local.gpu_percent,
                         local.ram_used_gb, local.ram_total_gb);
                log_counter = 0;
            }
        }

        xSemaphoreGive(lvgl_mutex);
        vTaskDelay(pdMS_TO_TICKS(100)); // 10 Hz update rate
    }
}

/* ============================================================================
 * TASKS: LVGL TICK & TIMER
 * ========================================================================== */
static void lvgl_tick_task(void *arg) {
    while(1) {
        lv_tick_inc(5);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

static void lvgl_timer_task(void *arg) {
    while(1) {
        xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
        lv_timer_handler();
        xSemaphoreGive(lvgl_mutex);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ============================================================================
 * MAIN
 * ========================================================================== */
void app_main(void) {
    ESP_LOGI(TAG, "=== PC Monitor - Desert-Spec Edition ===");

    /* Disable Task Watchdog */
    esp_task_wdt_deinit();

    /* Create Mutexes */
    stats_mutex = xSemaphoreCreateMutex();
    lvgl_mutex = xSemaphoreCreateMutex();

    /* Init USB Serial */
    usb_serial_jtag_driver_config_t usb_cfg = {
        .rx_buffer_size = 2048,
        .tx_buffer_size = 1024
    };
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_cfg));
    ESP_LOGI(TAG, "USB Serial initialized");

    /* Init SPI Bus */
    spi_bus_config_t buscfg = {
        .mosi_io_num = 5,
        .sclk_io_num = 4,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 240 * 240 * 2
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    ESP_LOGI(TAG, "SPI Bus initialized");

    /* Init LVGL */
    lv_init();
    ESP_LOGI(TAG, "LVGL initialized");

    /* Create Displays & Screens (with mutex protection) */
    xSemaphoreTake(lvgl_mutex, portMAX_DELAY);

    ESP_LOGI(TAG, "Creating Display 1: CPU");
    ESP_ERROR_CHECK(lvgl_gc9a01_init(&config_cpu, &display_cpu));
    screen_cpu = screen_cpu_create(lvgl_gc9a01_get_display(&display_cpu));

    ESP_LOGI(TAG, "Creating Display 2: GPU");
    ESP_ERROR_CHECK(lvgl_gc9a01_init(&config_gpu, &display_gpu));
    screen_gpu = screen_gpu_create(lvgl_gc9a01_get_display(&display_gpu));

    ESP_LOGI(TAG, "Creating Display 3: RAM");
    ESP_ERROR_CHECK(lvgl_gc9a01_init(&config_ram, &display_ram));
    screen_ram = screen_ram_create(lvgl_gc9a01_get_display(&display_ram));

    ESP_LOGI(TAG, "Creating Display 4: Network");
    ESP_ERROR_CHECK(lvgl_gc9a01_init(&config_net, &display_network));
    screen_network = screen_network_create(lvgl_gc9a01_get_display(&display_network));

    xSemaphoreGive(lvgl_mutex);

    /* Memory Stats */
    ESP_LOGI(TAG, "Free PSRAM: %d KB", heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024);
    ESP_LOGI(TAG, "Free Internal: %d KB", heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024);

    /* Start Tasks */
    xTaskCreate(usb_rx_task, "usb_rx", 4096, NULL, 10, NULL);
    xTaskCreate(lvgl_tick_task, "lv_tick", 2048, NULL, 20, NULL);
    xTaskCreatePinnedToCore(lvgl_timer_task, "lv_timer", 16384, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(display_update_task, "update", 8192, NULL, 4, NULL, 0);

    ESP_LOGI(TAG, "=== System Ready! ===");
}
