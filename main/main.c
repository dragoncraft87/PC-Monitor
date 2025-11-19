#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/usb_serial_jtag.h"
#include "gc9a01.h"
#include "graphics.h"
#include "screens.h"

static const char *TAG = "PC-MONITOR";

// Display handles
static gc9a01_handle_t display_cpu;
static gc9a01_handle_t display_gpu;
static gc9a01_handle_t display_ram;
static gc9a01_handle_t display_network;

// PC stats
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

// GPIO Pin definitions for 4 displays
// Custom wiring by Richard
static const gc9a01_pins_t pins_cpu = {
    .sck = 4,
    .mosi = 5,
    .cs = 11,
    .dc = 12,
    .rst = 13
};

static const gc9a01_pins_t pins_gpu = {
    .sck = 4,
    .mosi = 5,
    .cs = 10,
    .dc = 9,
    .rst = 46
};

static const gc9a01_pins_t pins_ram = {
    .sck = 4,
    .mosi = 5,
    .cs = 3,
    .dc = 8,
    .rst = 18
};

static const gc9a01_pins_t pins_network = {
    .sck = 4,
    .mosi = 5,
    .cs = 15,
    .dc = 16,
    .rst = 17
};

// USB Serial receive buffer
#define USB_RX_BUF_SIZE 512
static uint8_t usb_rx_buf[USB_RX_BUF_SIZE];

void parse_pc_data(const char *data) {
    // Parse JSON-like data from PC
    // Format: "CPU:45,CPUT:62.5,GPU:72,GPUT:68.3,VRAM:4.2/8.0,RAM:10.4/16.0,NET:LAN,SPEED:1000,DOWN:12.4,UP:2.1"
    
    char *token;
    char data_copy[USB_RX_BUF_SIZE];
    strncpy(data_copy, data, USB_RX_BUF_SIZE - 1);
    
    token = strtok(data_copy, ",");
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
}

void usb_rx_task(void *arg) {
    ESP_LOGI(TAG, "USB RX Task started");
    
    while (1) {
        int len = usb_serial_jtag_read_bytes(usb_rx_buf, USB_RX_BUF_SIZE - 1, pdMS_TO_TICKS(100));
        if (len > 0) {
            usb_rx_buf[len] = '\0';
            ESP_LOGI(TAG, "Received: %s", usb_rx_buf);
            parse_pc_data((char *)usb_rx_buf);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void display_update_task(void *arg) {
    ESP_LOGI(TAG, "Display update task started");
    
    while (1) {
        ESP_LOGI(TAG, "=== UPDATE CYCLE START ===");
        
        ESP_LOGI(TAG, "Updating CPU display...");
        screen_cpu_update(&display_cpu, &pc_stats);
        vTaskDelay(pdMS_TO_TICKS(50));
        
        ESP_LOGI(TAG, "Updating GPU display...");
        screen_gpu_update(&display_gpu, &pc_stats);
        vTaskDelay(pdMS_TO_TICKS(50));
        
        ESP_LOGI(TAG, "Updating RAM display...");
        screen_ram_update(&display_ram, &pc_stats);
        vTaskDelay(pdMS_TO_TICKS(50));
        
        ESP_LOGI(TAG, "Updating Network display...");
        screen_network_update(&display_network, &pc_stats);
        
        ESP_LOGI(TAG, "=== UPDATE CYCLE END ===");
        vTaskDelay(pdMS_TO_TICKS(800));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "PC Monitor 4x Display starting...");
    
    // Configure USB Serial
    usb_serial_jtag_driver_config_t usb_config = {
        .rx_buffer_size = USB_RX_BUF_SIZE,
        .tx_buffer_size = 1024,
    };
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_config));
    
    // Initialize SPI bus (shared by all displays)
    spi_bus_config_t buscfg = {
        .mosi_io_num = 5,
        .miso_io_num = -1,
        .sclk_io_num = 4,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = GC9A01_WIDTH * GC9A01_HEIGHT * 2
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    
    // Initialize Display 1: CPU
    ESP_LOGI(TAG, "Initializing CPU display...");
    ESP_ERROR_CHECK(gc9a01_init(&display_cpu, &pins_cpu, SPI2_HOST));
    screen_cpu_init(&display_cpu);
    
    // Initialize Display 2: GPU
    ESP_LOGI(TAG, "Initializing GPU display...");
    ESP_ERROR_CHECK(gc9a01_init(&display_gpu, &pins_gpu, SPI2_HOST));
    screen_gpu_init(&display_gpu);
    
    // Initialize Display 3: RAM
    ESP_LOGI(TAG, "Initializing RAM display...");
    ESP_ERROR_CHECK(gc9a01_init(&display_ram, &pins_ram, SPI2_HOST));
    screen_ram_init(&display_ram);
    
    // Initialize Display 4: Network
    ESP_LOGI(TAG, "Initializing Network display...");
    ESP_ERROR_CHECK(gc9a01_init(&display_network, &pins_network, SPI2_HOST));
    screen_network_init(&display_network);
    
    ESP_LOGI(TAG, "All displays initialized!");
    
    ESP_LOGI(TAG, "Drawing test pattern on CPU display...");
    gc9a01_fill_screen(&display_cpu, COLOR_RED);
    vTaskDelay(pdMS_TO_TICKS(1000));
    gc9a01_fill_screen(&display_cpu, COLOR_GREEN);
    vTaskDelay(pdMS_TO_TICKS(1000));
    gc9a01_fill_screen(&display_cpu, COLOR_BLUE);
    vTaskDelay(pdMS_TO_TICKS(1000));
    gc9a01_fill_screen(&display_cpu, COLOR_BLACK);
    ESP_LOGI(TAG, "Test pattern done!");
    
    // Create tasks
    xTaskCreate(usb_rx_task, "usb_rx", 4096, NULL, 10, NULL);
    xTaskCreate(display_update_task, "display_update", 8192, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "System ready!");
}
