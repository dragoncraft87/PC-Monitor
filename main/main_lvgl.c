/**
 * @file main_lvgl.c
 * @brief PC Monitor - 4x GC9A01 Displays with LVGL
 *
 * ESP32-S3 N16R8 with LVGL for 4 round displays:
 * - Display 1 (CPU): Ring gauge with percentage and temperature
 * - Display 2 (GPU): Ring gauge with percentage, temp, VRAM
 * - Display 3 (RAM): Horizontal bar with segments
 * - Display 4 (Network): Cyberpunk graph with traffic history
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
 * PC STATS
 * ========================================================================== */
static pc_stats_t pc_stats = {
    .cpu_percent = 45,
    .cpu_temp = 62.5,
    .gpu_percent = 72,
    .gpu_temp = 68.3,
    .gpu_vram_used = 4.2,
    .gpu_vram_total = 8.0,
    .ram_used_gb = 10.4,
    .ram_total_gb = 16.0,
    .net_type = "LAN",
    .net_speed = "1000 Mbps",
    .net_down_mbps = 12.4,
    .net_up_mbps = 2.1
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
 * USB SERIAL RECEIVE BUFFER
 * ========================================================================== */
#define USB_RX_BUF_SIZE 512
static uint8_t usb_rx_buf[USB_RX_BUF_SIZE];

/**
 * @brief Parse PC data from USB Serial
 * Format: "CPU:45,CPUT:62.5,GPU:72,GPUT:68.3,VRAM:4.2/8.0,RAM:10.4/16.0,NET:LAN,SPEED:1000,DOWN:12.4,UP:2.1"
 */
void parse_pc_data(const char *data)
{
    char data_copy[USB_RX_BUF_SIZE];
    strncpy(data_copy, data, USB_RX_BUF_SIZE - 1);

    xSemaphoreTake(stats_mutex, portMAX_DELAY);

    char *token = strtok(data_copy, ",");
    while (token != NULL) {
        if (strncmp(token, "CPU:", 4) == 0) {
            pc_stats.cpu_percent = atoi(token + 4);
        } else if (strncmp(token, "CPUT:", 5) == 0) {
            pc_stats.cpu_temp = atof(token + 5);
        } else if (strncmp(token, "GPU:", 4) == 0) {
            pc_stats.gpu_percent = atoi(token + 4);
        } else if (strncmp(token, "GPUT:", 5) == 0) {
            pc_stats.gpu_temp = atof(token + 5);
        } else if (strncmp(token, "VRAM:", 5) == 0) {
            sscanf(token + 5, "%f/%f", &pc_stats.gpu_vram_used, &pc_stats.gpu_vram_total);
        } else if (strncmp(token, "RAM:", 4) == 0) {
            sscanf(token + 4, "%f/%f", &pc_stats.ram_used_gb, &pc_stats.ram_total_gb);
        } else if (strncmp(token, "NET:", 4) == 0) {
            strncpy(pc_stats.net_type, token + 4, 15);
        } else if (strncmp(token, "SPEED:", 6) == 0) {
            strncpy(pc_stats.net_speed, token + 6, 15);
        } else if (strncmp(token, "DOWN:", 5) == 0) {
            pc_stats.net_down_mbps = atof(token + 5);
        } else if (strncmp(token, "UP:", 3) == 0) {
            pc_stats.net_up_mbps = atof(token + 3);
        }
        token = strtok(NULL, ",");
    }

    xSemaphoreGive(stats_mutex);
}

/**
 * @brief USB RX Task - Receives data from PC
 */
void usb_rx_task(void *arg)
{
    ESP_LOGI(TAG, "USB RX Task started - waiting for data...");
    int no_data_counter = 0;

    while (1) {
        int len = usb_serial_jtag_read_bytes(usb_rx_buf, USB_RX_BUF_SIZE - 1, pdMS_TO_TICKS(100));
        if (len > 0) {
            usb_rx_buf[len] = '\0';
            ESP_LOGI(TAG, "✓ RX [%d bytes]: %s", len, usb_rx_buf);
            parse_pc_data((char *)usb_rx_buf);
            no_data_counter = 0;  // Reset counter
        } else {
            no_data_counter++;
            if (no_data_counter >= 100) {  // Alle 10 Sekunden
                ESP_LOGW(TAG, "⚠ No data received for 10 seconds - check Python script!");
                no_data_counter = 0;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

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
 *
 * CRITICAL FIX: Added NULL-pointer checks to prevent crashes if screen creation fails
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

        /* Lock LVGL mutex ONCE for all screen updates to keep frame consistent */
        if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            /* NULL-pointer checks to prevent crashes */
            if (screen_cpu) screen_cpu_update(screen_cpu, &stats_copy);
            if (screen_gpu) screen_gpu_update(screen_gpu, &stats_copy);
            if (screen_ram) screen_ram_update(screen_ram, &stats_copy);
            if (screen_network) screen_network_update(screen_network, &stats_copy);
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

    /* ========================================================================
     * STEP 5: Create Tasks
     * ====================================================================== */
    /* Note: Stack sizes increased to prevent overflow with 4 displays + LVGL 9
     * Stack is allocated from PSRAM (CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY=y)
     */
    xTaskCreate(lvgl_tick_task, "lvgl_tick", 2048, NULL, 8, NULL); /* 2KB - Prio 8 (reduced from 10) */
    xTaskCreatePinnedToCore(lvgl_timer_task, "lvgl_timer", 32768, NULL, 5, NULL, 1);  /* Core 1 - 32KB (was 24KB) */
    xTaskCreate(usb_rx_task, "usb_rx", 6144, NULL, 6, NULL);  /* 6KB - Prio 6 (reduced from 10) */
    xTaskCreatePinnedToCore(display_update_task, "display_update", 20480, NULL, 4, NULL, 0);  /* Core 0 - 20KB (was 16KB) */

    ESP_LOGI(TAG, "=== System ready! ===");
}
