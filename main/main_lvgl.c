/**
 * @file main_lvgl.c
 * @brief PC Monitor - 4x GC9A01 Displays with LVGL
 *
 * "Desert-Spec" Edition - Enterprise-grade stability
 *
 * ESP32-S3 N16R8 with LVGL for 4 round displays:
 * - Display 1 (CPU): Ring gauge with percentage and temperature
 * - Display 2 (GPU): Ring gauge with percentage, temp, VRAM
 * - Display 3 (RAM): Horizontal bar with segments
 * - Display 4 (Network): Cyberpunk graph with traffic history
 *
 * Features:
 * - Robust USB line-buffer parser (handles partial packets)
 * - Screensaver mode after 5s without data
 * - Thread-safe operations with mutexes
 * - PSRAM-first memory allocation strategy
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

#include "lvgl.h"
#include "lvgl_gc9a01_driver.h"
#include "screens/screens_lvgl.h"

static const char *TAG = "PC-MONITOR-LVGL";

/* ============================================================================
 * CUSTOM MEMORY ALLOCATOR - PREFER PSRAM OVER INTERNAL RAM
 * ========================================================================== */

/**
 * @brief Custom malloc that prefers PSRAM to save internal RAM
 *
 * Strategy:
 * 1. Try PSRAM first (for most LVGL objects)
 * 2. Fall back to DMA-capable internal RAM if PSRAM fails
 * 3. Fall back to any internal RAM as last resort
 */
void *lv_custom_malloc(size_t size)
{
    if (size == 0) return NULL;

    /* Try PSRAM first (best for large allocations) */
    void *ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (ptr) {
        return ptr;
    }

    /* PSRAM full - try DMA-capable internal RAM */
    ptr = heap_caps_malloc(size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (ptr) {
        ESP_LOGW(TAG, "PSRAM full! Allocated %d bytes from internal DMA RAM", size);
        return ptr;
    }

    /* Last resort: any internal RAM */
    ptr = heap_caps_malloc(size, MALLOC_CAP_INTERNAL);
    if (ptr) {
        ESP_LOGW(TAG, "PSRAM full! Allocated %d bytes from internal RAM", size);
        return ptr;
    }

    ESP_LOGE(TAG, "OUT OF MEMORY! Failed to allocate %d bytes", size);
    return NULL;
}

/**
 * @brief Custom free (works with any heap type)
 */
void lv_custom_free(void *ptr)
{
    if (ptr) {
        heap_caps_free(ptr);
    }
}

/**
 * @brief Custom realloc that prefers PSRAM
 */
void *lv_custom_realloc(void *ptr, size_t size)
{
    if (size == 0) {
        lv_custom_free(ptr);
        return NULL;
    }

    if (!ptr) {
        return lv_custom_malloc(size);
    }

    /* Try realloc in PSRAM first */
    void *new_ptr = heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM);
    if (new_ptr) {
        return new_ptr;
    }

    /* PSRAM realloc failed - allocate new block and copy */
    new_ptr = lv_custom_malloc(size);
    if (new_ptr) {
        /* Copy old data (we don't know the old size, so copy min of new size) */
        memcpy(new_ptr, ptr, size);
        lv_custom_free(ptr);
        return new_ptr;
    }

    ESP_LOGE(TAG, "OUT OF MEMORY! Failed to realloc %d bytes", size);
    return NULL;
}

/* ============================================================================
 * DISPLAY HANDLES
 * ========================================================================== */
static lvgl_gc9a01_handle_t display_cpu;
static lvgl_gc9a01_handle_t display_gpu;
static lvgl_gc9a01_handle_t display_ram;
static lvgl_gc9a01_handle_t display_network;

/* Screen handles */
static screen_cpu_t *screen_cpu;
static screen_gpu_t *screen_gpu;
static screen_ram_t *screen_ram;
static screen_network_t *screen_network;

/* ============================================================================
 * SCREENSAVER STATE
 * ========================================================================== */
#define SCREENSAVER_TIMEOUT_MS  5000   /* 5 seconds without data -> screensaver */

static volatile bool screensaver_active = false;
static volatile uint32_t last_data_timestamp = 0;

/* Screensaver image objects (one per display) */
static lv_obj_t *screensaver_img_cpu = NULL;
static lv_obj_t *screensaver_img_gpu = NULL;
static lv_obj_t *screensaver_img_ram = NULL;
static lv_obj_t *screensaver_img_network = NULL;

/* Placeholder image declaration - replace with actual images later */
LV_IMG_DECLARE(img_screensaver_placeholder);

/* ============================================================================
 * PC STATS
 * ========================================================================== */
static pc_stats_t pc_stats = {
    .cpu_percent = 0,
    .cpu_temp = 0.0,
    .gpu_percent = 0,
    .gpu_temp = 0.0,
    .gpu_vram_used = 0.0,
    .gpu_vram_total = 12.0,
    .ram_used_gb = 0.0,
    .ram_total_gb = 64.0,
    .net_type = "---",
    .net_speed = "---",
    .net_down_mbps = 0.0,
    .net_up_mbps = 0.0
};

/* Mutex for thread-safe access to pc_stats */
static SemaphoreHandle_t stats_mutex;

/* Mutex for thread-safe LVGL operations */
static SemaphoreHandle_t lvgl_mutex;

/* ============================================================================
 * GPIO PIN DEFINITIONS (Custom wiring by Richard)
 * ========================================================================== */
static const lvgl_gc9a01_config_t config_cpu = {
    .pin_sck = 4,
    .pin_mosi = 5,
    .pin_cs = 12,
    .pin_dc = 11,
    .pin_rst = 13,
    .spi_host = SPI2_HOST
};

static const lvgl_gc9a01_config_t config_gpu = {
    .pin_sck = 4,
    .pin_mosi = 5,
    .pin_cs = 9,
    .pin_dc = 46,
    .pin_rst = 10,
    .spi_host = SPI2_HOST
};

static const lvgl_gc9a01_config_t config_ram = {
    .pin_sck = 4,
    .pin_mosi = 5,
    .pin_cs = 8,
    .pin_dc = 18,
    .pin_rst = 3,
    .spi_host = SPI2_HOST
};

static const lvgl_gc9a01_config_t config_network = {
    .pin_sck = 4,
    .pin_mosi = 5,
    .pin_cs = 16,
    .pin_dc = 15,
    .pin_rst = 17,
    .spi_host = SPI2_HOST
};

/* ============================================================================
 * ROBUST USB LINE-BUFFER PARSER
 * ========================================================================== */
#define USB_RX_BUF_SIZE     512
#define USB_LINE_BUF_SIZE   1024

static uint8_t usb_rx_chunk[USB_RX_BUF_SIZE];  /* Chunk buffer for USB reads */
static char usb_line_buffer[USB_LINE_BUF_SIZE]; /* Accumulated line buffer */
static size_t usb_line_pos = 0;                 /* Current position in line buffer */

/**
 * @brief Parse PC data from a complete line
 * Format: "CPU:45,CPUT:62.5,GPU:72,GPUT:68.3,VRAM:4.2/8.0,RAM:10.4/16.0,NET:LAN,SPEED:1000,DOWN:12.4,UP:2.1"
 *
 * @param line Complete line to parse (null-terminated, without newline)
 * @return true if parsing succeeded and data was valid
 */
static bool parse_pc_data_line(const char *line)
{
    if (!line || strlen(line) < 10) {
        return false;  /* Line too short to be valid */
    }

    /* Make a copy for tokenization */
    char data_copy[USB_LINE_BUF_SIZE];
    strncpy(data_copy, line, USB_LINE_BUF_SIZE - 1);
    data_copy[USB_LINE_BUF_SIZE - 1] = '\0';

    /* Temporary stats - only apply if we parsed at least some valid fields */
    pc_stats_t temp_stats;
    int valid_fields = 0;

    /* Lock mutex to get current stats as baseline */
    xSemaphoreTake(stats_mutex, portMAX_DELAY);
    memcpy(&temp_stats, &pc_stats, sizeof(pc_stats_t));
    xSemaphoreGive(stats_mutex);

    char *token = strtok(data_copy, ",");
    while (token != NULL) {
        if (strncmp(token, "CPU:", 4) == 0) {
            temp_stats.cpu_percent = atoi(token + 4);
            valid_fields++;
        } else if (strncmp(token, "CPUT:", 5) == 0) {
            temp_stats.cpu_temp = atof(token + 5);
            valid_fields++;
        } else if (strncmp(token, "GPU:", 4) == 0) {
            temp_stats.gpu_percent = atoi(token + 4);
            valid_fields++;
        } else if (strncmp(token, "GPUT:", 5) == 0) {
            temp_stats.gpu_temp = atof(token + 5);
            valid_fields++;
        } else if (strncmp(token, "VRAM:", 5) == 0) {
            sscanf(token + 5, "%f/%f", &temp_stats.gpu_vram_used, &temp_stats.gpu_vram_total);
            valid_fields++;
        } else if (strncmp(token, "RAM:", 4) == 0) {
            sscanf(token + 4, "%f/%f", &temp_stats.ram_used_gb, &temp_stats.ram_total_gb);
            valid_fields++;
        } else if (strncmp(token, "NET:", 4) == 0) {
            strncpy(temp_stats.net_type, token + 4, 15);
            temp_stats.net_type[15] = '\0';
            valid_fields++;
        } else if (strncmp(token, "SPEED:", 6) == 0) {
            strncpy(temp_stats.net_speed, token + 6, 15);
            temp_stats.net_speed[15] = '\0';
            valid_fields++;
        } else if (strncmp(token, "DOWN:", 5) == 0) {
            temp_stats.net_down_mbps = atof(token + 5);
            valid_fields++;
        } else if (strncmp(token, "UP:", 3) == 0) {
            temp_stats.net_up_mbps = atof(token + 3);
            valid_fields++;
        }
        token = strtok(NULL, ",");
    }

    /* Only update if we got at least 3 valid fields */
    if (valid_fields >= 3) {
        xSemaphoreTake(stats_mutex, portMAX_DELAY);
        memcpy(&pc_stats, &temp_stats, sizeof(pc_stats_t));
        xSemaphoreGive(stats_mutex);
        return true;
    }

    return false;
}

/**
 * @brief Process USB receive buffer with line-based parsing
 *
 * This function implements a robust line-buffer that:
 * - Accumulates bytes until a complete line (\n) is received
 * - Handles partial packets gracefully
 * - Preserves remaining bytes for the next cycle
 * - Resets buffer on overflow to prevent memory corruption
 */
static void process_usb_rx(void)
{
    /* Read available bytes into chunk buffer */
    int len = usb_serial_jtag_read_bytes(usb_rx_chunk, USB_RX_BUF_SIZE - 1, pdMS_TO_TICKS(50));

    if (len <= 0) {
        return;  /* No data available */
    }

    /* Process each received byte */
    for (int i = 0; i < len; i++) {
        char c = (char)usb_rx_chunk[i];

        /* Check for line terminator */
        if (c == '\n' || c == '\r') {
            if (usb_line_pos > 0) {
                /* Null-terminate the line */
                usb_line_buffer[usb_line_pos] = '\0';

                /* Parse the complete line */
                if (parse_pc_data_line(usb_line_buffer)) {
                    /* Update timestamp on successful parse */
                    last_data_timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;
                    ESP_LOGD(TAG, "✓ Parsed: %s", usb_line_buffer);
                } else {
                    ESP_LOGW(TAG, "⚠ Invalid data: %s", usb_line_buffer);
                }

                /* Reset line buffer for next line */
                usb_line_pos = 0;
            }
            /* Skip empty lines (consecutive \r\n) */
            continue;
        }

        /* Add character to line buffer */
        if (usb_line_pos < USB_LINE_BUF_SIZE - 1) {
            usb_line_buffer[usb_line_pos++] = c;
        } else {
            /* Buffer overflow - reset and log error */
            ESP_LOGE(TAG, "Line buffer overflow! Resetting...");
            usb_line_pos = 0;
        }
    }
}

/**
 * @brief USB RX Task - Receives data from PC with robust line parsing
 */
void usb_rx_task(void *arg)
{
    ESP_LOGI(TAG, "USB RX Task started - waiting for data...");

    /* Initialize last data timestamp */
    last_data_timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;

    while (1) {
        process_usb_rx();
        vTaskDelay(pdMS_TO_TICKS(50));  /* 50ms polling interval */
    }
}

/* ============================================================================
 * SCREENSAVER FUNCTIONS
 * ========================================================================== */

/**
 * @brief Show screensaver on CPU display (Sonic Theme)
 */
static void show_screensaver_cpu(void)
{
    if (!screen_cpu || screensaver_img_cpu) return;

    lv_display_t *disp = lvgl_gc9a01_get_display(&display_cpu);
    lv_display_t *old_default = lv_display_get_default();
    lv_display_set_default(disp);

    /* Hide all screen elements */
    lv_obj_t *scr = lv_display_get_screen_active(disp);
    lv_obj_add_flag(scr, LV_OBJ_FLAG_HIDDEN);

    /* Create screensaver screen */
    lv_obj_t *ss_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ss_screen, lv_color_hex(0x000033), 0);  /* Sonic blue tint */

    /* Add placeholder image (Sonic Theme) */
    screensaver_img_cpu = lv_label_create(ss_screen);
    lv_label_set_text(screensaver_img_cpu, "SONIC");
    lv_obj_set_style_text_color(screensaver_img_cpu, lv_color_hex(0x0066FF), 0);
    lv_obj_set_style_text_font(screensaver_img_cpu, &lv_font_montserrat_24, 0);
    lv_obj_center(screensaver_img_cpu);

    lv_screen_load(ss_screen);

    if (old_default) lv_display_set_default(old_default);
}

/**
 * @brief Show screensaver on GPU display (Painter Theme)
 */
static void show_screensaver_gpu(void)
{
    if (!screen_gpu || screensaver_img_gpu) return;

    lv_display_t *disp = lvgl_gc9a01_get_display(&display_gpu);
    lv_display_t *old_default = lv_display_get_default();
    lv_display_set_default(disp);

    lv_obj_t *scr = lv_display_get_screen_active(disp);
    lv_obj_add_flag(scr, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *ss_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ss_screen, lv_color_hex(0x1A0033), 0);  /* Purple tint */

    /* Add placeholder (Painter Theme) */
    screensaver_img_gpu = lv_label_create(ss_screen);
    lv_label_set_text(screensaver_img_gpu, "PAINTER");
    lv_obj_set_style_text_color(screensaver_img_gpu, lv_color_hex(0xFF6600), 0);
    lv_obj_set_style_text_font(screensaver_img_gpu, &lv_font_montserrat_24, 0);
    lv_obj_center(screensaver_img_gpu);

    lv_screen_load(ss_screen);

    if (old_default) lv_display_set_default(old_default);
}

/**
 * @brief Show screensaver on RAM display (Donkey Kong Theme)
 */
static void show_screensaver_ram(void)
{
    if (!screen_ram || screensaver_img_ram) return;

    lv_display_t *disp = lvgl_gc9a01_get_display(&display_ram);
    lv_display_t *old_default = lv_display_get_default();
    lv_display_set_default(disp);

    lv_obj_t *scr = lv_display_get_screen_active(disp);
    lv_obj_add_flag(scr, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *ss_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ss_screen, lv_color_hex(0x331100), 0);  /* Brown tint */

    /* Add placeholder (Donkey Kong Theme) */
    screensaver_img_ram = lv_label_create(ss_screen);
    lv_label_set_text(screensaver_img_ram, "DK");
    lv_obj_set_style_text_color(screensaver_img_ram, lv_color_hex(0xFFCC00), 0);
    lv_obj_set_style_text_font(screensaver_img_ram, &lv_font_montserrat_42, 0);
    lv_obj_center(screensaver_img_ram);

    lv_screen_load(ss_screen);

    if (old_default) lv_display_set_default(old_default);
}

/**
 * @brief Show screensaver on Network display (Pac-Man Theme)
 */
static void show_screensaver_network(void)
{
    if (!screen_network || screensaver_img_network) return;

    lv_display_t *disp = lvgl_gc9a01_get_display(&display_network);
    lv_display_t *old_default = lv_display_get_default();
    lv_display_set_default(disp);

    lv_obj_t *scr = lv_display_get_screen_active(disp);
    lv_obj_add_flag(scr, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *ss_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ss_screen, lv_color_hex(0x000011), 0);  /* Dark blue */

    /* Add placeholder (Pac-Man Theme) */
    screensaver_img_network = lv_label_create(ss_screen);
    lv_label_set_text(screensaver_img_network, "PAC-MAN");
    lv_obj_set_style_text_color(screensaver_img_network, lv_color_hex(0xFFFF00), 0);
    lv_obj_set_style_text_font(screensaver_img_network, &lv_font_montserrat_20, 0);
    lv_obj_center(screensaver_img_network);

    lv_screen_load(ss_screen);

    if (old_default) lv_display_set_default(old_default);
}

/**
 * @brief Hide screensaver and restore normal display on CPU
 */
static void hide_screensaver_cpu(void)
{
    if (!screensaver_img_cpu) return;

    lv_display_t *disp = lvgl_gc9a01_get_display(&display_cpu);
    lv_display_t *old_default = lv_display_get_default();
    lv_display_set_default(disp);

    /* Delete screensaver screen and restore original */
    lv_obj_t *ss_screen = lv_obj_get_parent(screensaver_img_cpu);

    /* Get the original screen from screen_cpu and reload it */
    extern lv_obj_t *screen_cpu_get_screen(screen_cpu_t *s);
    lv_obj_t *original_screen = screen_cpu_get_screen(screen_cpu);
    if (original_screen) {
        lv_obj_remove_flag(original_screen, LV_OBJ_FLAG_HIDDEN);
        lv_screen_load(original_screen);
    }

    /* Delete screensaver screen */
    lv_obj_delete(ss_screen);
    screensaver_img_cpu = NULL;

    if (old_default) lv_display_set_default(old_default);
}

/**
 * @brief Hide screensaver on GPU
 */
static void hide_screensaver_gpu(void)
{
    if (!screensaver_img_gpu) return;

    lv_display_t *disp = lvgl_gc9a01_get_display(&display_gpu);
    lv_display_t *old_default = lv_display_get_default();
    lv_display_set_default(disp);

    lv_obj_t *ss_screen = lv_obj_get_parent(screensaver_img_gpu);

    extern lv_obj_t *screen_gpu_get_screen(screen_gpu_t *s);
    lv_obj_t *original_screen = screen_gpu_get_screen(screen_gpu);
    if (original_screen) {
        lv_obj_remove_flag(original_screen, LV_OBJ_FLAG_HIDDEN);
        lv_screen_load(original_screen);
    }

    lv_obj_delete(ss_screen);
    screensaver_img_gpu = NULL;

    if (old_default) lv_display_set_default(old_default);
}

/**
 * @brief Hide screensaver on RAM
 */
static void hide_screensaver_ram(void)
{
    if (!screensaver_img_ram) return;

    lv_display_t *disp = lvgl_gc9a01_get_display(&display_ram);
    lv_display_t *old_default = lv_display_get_default();
    lv_display_set_default(disp);

    lv_obj_t *ss_screen = lv_obj_get_parent(screensaver_img_ram);

    extern lv_obj_t *screen_ram_get_screen(screen_ram_t *s);
    lv_obj_t *original_screen = screen_ram_get_screen(screen_ram);
    if (original_screen) {
        lv_obj_remove_flag(original_screen, LV_OBJ_FLAG_HIDDEN);
        lv_screen_load(original_screen);
    }

    lv_obj_delete(ss_screen);
    screensaver_img_ram = NULL;

    if (old_default) lv_display_set_default(old_default);
}

/**
 * @brief Hide screensaver on Network
 */
static void hide_screensaver_network(void)
{
    if (!screensaver_img_network) return;

    lv_display_t *disp = lvgl_gc9a01_get_display(&display_network);
    lv_display_t *old_default = lv_display_get_default();
    lv_display_set_default(disp);

    lv_obj_t *ss_screen = lv_obj_get_parent(screensaver_img_network);

    extern lv_obj_t *screen_network_get_screen(screen_network_t *s);
    lv_obj_t *original_screen = screen_network_get_screen(screen_network);
    if (original_screen) {
        lv_obj_remove_flag(original_screen, LV_OBJ_FLAG_HIDDEN);
        lv_screen_load(original_screen);
    }

    lv_obj_delete(ss_screen);
    screensaver_img_network = NULL;

    if (old_default) lv_display_set_default(old_default);
}

/**
 * @brief Activate screensaver on all displays
 */
static void screensaver_activate(void)
{
    if (screensaver_active) return;

    ESP_LOGI(TAG, "🌙 Activating screensaver (no data for %d ms)", SCREENSAVER_TIMEOUT_MS);
    screensaver_active = true;

    show_screensaver_cpu();
    show_screensaver_gpu();
    show_screensaver_ram();
    show_screensaver_network();
}

/**
 * @brief Deactivate screensaver and restore normal displays
 */
static void screensaver_deactivate(void)
{
    if (!screensaver_active) return;

    ESP_LOGI(TAG, "☀️ Deactivating screensaver - data received!");
    screensaver_active = false;

    hide_screensaver_cpu();
    hide_screensaver_gpu();
    hide_screensaver_ram();
    hide_screensaver_network();
}

/**
 * @brief Check screensaver timeout and manage state
 */
static void screensaver_check(void)
{
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint32_t time_since_data = current_time - last_data_timestamp;

    if (time_since_data > SCREENSAVER_TIMEOUT_MS) {
        if (!screensaver_active) {
            screensaver_activate();
        }
    } else {
        if (screensaver_active) {
            screensaver_deactivate();
        }
    }
}

/* ============================================================================
 * LVGL TASKS
 * ========================================================================== */

/**
 * @brief LVGL Tick Task - Provides time base for LVGL
 */
void lvgl_tick_task(void *arg)
{
    while (1) {
        lv_tick_inc(10);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * @brief LVGL Timer Task - Handles LVGL internal timers
 * Protected by mutex to prevent race conditions with display updates
 */
void lvgl_timer_task(void *arg)
{
    while (1) {
        /* Lock LVGL mutex before calling timer handler */
        if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            uint32_t time_till_next = lv_timer_handler();
            xSemaphoreGive(lvgl_mutex);

            /* Yield to other tasks to prevent watchdog */
            if (time_till_next > 10) {
                vTaskDelay(pdMS_TO_TICKS(10));
            } else {
                vTaskDelay(pdMS_TO_TICKS(time_till_next));
            }
        } else {
            /* Mutex timeout - just wait and retry */
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

/**
 * @brief Display Update Task - Updates all 4 displays
 * All screen updates are protected by LVGL mutex to ensure consistency
 */
void display_update_task(void *arg)
{
    ESP_LOGI(TAG, "Display update task started");
    int update_counter = 0;

    while (1) {
        /* Copy stats to local variable (fast) */
        xSemaphoreTake(stats_mutex, portMAX_DELAY);
        pc_stats_t stats_copy = pc_stats;
        xSemaphoreGive(stats_mutex);

        /* Debug: Log current values every 10 seconds */
        if (update_counter % 10 == 0) {
            ESP_LOGI(TAG, "📊 Stats: CPU=%d%% (%.1f°C), GPU=%d%% (%.1f°C), RAM=%.1f/%.1fGB",
                     stats_copy.cpu_percent, stats_copy.cpu_temp,
                     stats_copy.gpu_percent, stats_copy.gpu_temp,
                     stats_copy.ram_used_gb, stats_copy.ram_total_gb);
        }
        update_counter++;

        /* Lock LVGL mutex ONCE for all operations */
        if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            /* Check and manage screensaver state */
            screensaver_check();

            /* Only update screens if screensaver is not active */
            if (!screensaver_active) {
                if (screen_cpu) screen_cpu_update(screen_cpu, &stats_copy);
                if (screen_gpu) screen_gpu_update(screen_gpu, &stats_copy);
                if (screen_ram) screen_ram_update(screen_ram, &stats_copy);
                if (screen_network) screen_network_update(screen_network, &stats_copy);
            }

            xSemaphoreGive(lvgl_mutex);
        } else {
            ESP_LOGW(TAG, "Failed to acquire LVGL mutex for display update!");
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); /* Update every second */
    }
}

/**
 * @brief Main application
 */
void app_main(void)
{
    ESP_LOGI(TAG, "=== PC Monitor 4x Display with LVGL ===");
    ESP_LOGI(TAG, "\"Desert-Spec\" Edition - Enterprise Stability");
    ESP_LOGI(TAG, "ESP32-S3 N16R8 with PSRAM");

    /* Disable Task Watchdog for now (LVGL needs more time) */
    ESP_ERROR_CHECK(esp_task_wdt_deinit());
    ESP_LOGI(TAG, "Task Watchdog disabled for LVGL");

    /* Create mutexes for thread safety */
    stats_mutex = xSemaphoreCreateMutex();
    lvgl_mutex = xSemaphoreCreateMutex();
    ESP_LOGI(TAG, "Mutexes created for thread-safe operation");

    /* ========================================================================
     * STEP 1: Configure USB Serial
     * ====================================================================== */
    usb_serial_jtag_driver_config_t usb_config = {
        .rx_buffer_size = USB_RX_BUF_SIZE,
        .tx_buffer_size = 1024,
    };
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_config));
    ESP_LOGI(TAG, "USB Serial JTAG configured");

    /* ========================================================================
     * STEP 2: Initialize SPI Bus (shared by all displays)
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
     * STEP 3: Initialize LVGL
     * ====================================================================== */
    ESP_LOGI(TAG, "Initializing LVGL...");
    lv_init();

    /* ========================================================================
     * STEP 4: Initialize 4 Displays with LVGL
     * IMPORTANT: Must be done BEFORE starting lvgl_timer_task to avoid race!
     * ====================================================================== */

    /* Lock LVGL mutex during display creation to prevent race with timer task */
    xSemaphoreTake(lvgl_mutex, portMAX_DELAY);

    /* Display 1: CPU */
    ESP_LOGI(TAG, "Initializing Display 1: CPU");
    ESP_ERROR_CHECK(lvgl_gc9a01_init(&config_cpu, &display_cpu));
    screen_cpu = screen_cpu_create(lvgl_gc9a01_get_display(&display_cpu));
    if (!screen_cpu) {
        ESP_LOGE(TAG, "Failed to create CPU screen!");
    }

    /* Display 2: GPU */
    ESP_LOGI(TAG, "Initializing Display 2: GPU");
    ESP_ERROR_CHECK(lvgl_gc9a01_init(&config_gpu, &display_gpu));
    screen_gpu = screen_gpu_create(lvgl_gc9a01_get_display(&display_gpu));
    if (!screen_gpu) {
        ESP_LOGE(TAG, "Failed to create GPU screen!");
    }

    /* Display 3: RAM */
    ESP_LOGI(TAG, "Initializing Display 3: RAM");
    ESP_ERROR_CHECK(lvgl_gc9a01_init(&config_ram, &display_ram));
    screen_ram = screen_ram_create(lvgl_gc9a01_get_display(&display_ram));
    if (!screen_ram) {
        ESP_LOGE(TAG, "Failed to create RAM screen!");
    }

    /* Display 4: Network */
    ESP_LOGI(TAG, "Initializing Display 4: Network");
    ESP_ERROR_CHECK(lvgl_gc9a01_init(&config_network, &display_network));
    screen_network = screen_network_create(lvgl_gc9a01_get_display(&display_network));
    if (!screen_network) {
        ESP_LOGE(TAG, "Failed to create Network screen!");
    }

    xSemaphoreGive(lvgl_mutex);

    /* Print memory statistics after initialization */
    ESP_LOGI(TAG, "=== Memory Statistics After Init ===");
    ESP_LOGI(TAG, "Free PSRAM: %d bytes", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    ESP_LOGI(TAG, "Free Internal RAM: %d bytes", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    ESP_LOGI(TAG, "Largest free PSRAM block: %d bytes", heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
    ESP_LOGI(TAG, "Largest free Internal block: %d bytes", heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));

    ESP_LOGI(TAG, "All 4 displays initialized!");

    /* Initialize screensaver timestamp */
    last_data_timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;

    /* ========================================================================
     * STEP 5: Create Tasks
     * ====================================================================== */
    /* Note: Stack sizes increased to prevent overflow with 4 displays + LVGL 9
     * Stack is allocated from PSRAM (CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY=y)
     */
    xTaskCreate(lvgl_tick_task, "lvgl_tick", 2048, NULL, 8, NULL);
    xTaskCreatePinnedToCore(lvgl_timer_task, "lvgl_timer", 32768, NULL, 5, NULL, 1);
    xTaskCreate(usb_rx_task, "usb_rx", 8192, NULL, 6, NULL);  /* Increased for line buffer */
    xTaskCreatePinnedToCore(display_update_task, "display_update", 24576, NULL, 4, NULL, 0);

    ESP_LOGI(TAG, "=== System ready! Waiting for USB data... ===");
    ESP_LOGI(TAG, "Screensaver will activate after %d ms without data", SCREENSAVER_TIMEOUT_MS);
}
