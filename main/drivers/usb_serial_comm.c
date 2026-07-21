/**
 * @file usb_serial_comm.c
 * @brief USB Serial Communication Driver Implementation
 */

#include "usb_serial_comm.h"
#include "../storage/hw_identity.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/usb_serial_jtag.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "esp_app_desc.h"

static const char *TAG = "USB-COMM";

/* Thread-safety configuration */
#define STATS_MUTEX_TIMEOUT_MS  100  /* Never block indefinitely! */

/* N/A hold ("glitch absorption"):
 * The PC client sends -1 for a sensor when a single read fails (e.g.
 * LibreHardwareMonitor returns null for one update cycle). Showing that
 * instantly makes the screen flick to "N/A" and back. Instead we keep the
 * last valid value for up to HOLD_MAX_PACKETS consecutive misses; only a
 * sustained outage (sensor really gone) is shown as N/A. */
#define HOLD_MAX_PACKETS  4

/* Global state */
static pc_stats_t s_pc_stats = {0};
static volatile uint32_t s_last_data_ms = 0;
static SemaphoreHandle_t s_stats_mutex = NULL;
static uint32_t s_stats_mutex_timeouts = 0;

/* Per-field miss counters for the N/A hold logic */
static struct {
    uint8_t cpu_pct, cpu_temp, gpu_pct, gpu_temp, vram, ram, net_dn, net_up;
} s_hold = {0};

/* If the new field is N/A (<0) and we haven't held too long, keep the old
 * value; otherwise accept the new value and reset the counter. */
#define HOLD_FIELD(newv, oldv, counter) do {          \
    if ((newv) < 0) {                                 \
        if ((counter) < HOLD_MAX_PACKETS) {           \
            (newv) = (oldv);                          \
            (counter)++;                              \
        }                                             \
    } else {                                          \
        (counter) = 0;                                \
    }                                                 \
} while (0)

/* Command handlers (max 8) */
#define MAX_CMD_HANDLERS 8
static usb_cmd_handler_t s_handlers[MAX_CMD_HANDLERS] = {0};
static int s_handler_count = 0;

/* =============================================================================
 * INITIALIZATION
 * ========================================================================== */

esp_err_t usb_serial_init(void)
{
    usb_serial_jtag_driver_config_t usb_cfg = {
        .rx_buffer_size = USB_RX_BUFFER_SIZE,
        .tx_buffer_size = USB_TX_BUFFER_SIZE
    };

    esp_err_t ret = usb_serial_jtag_driver_install(&usb_cfg);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "USB Serial JTAG initialized");
    } else {
        ESP_LOGE(TAG, "USB Serial JTAG init failed: %s", esp_err_to_name(ret));
    }

    return ret;
}

/* =============================================================================
 * ACCESSORS
 * ========================================================================== */

pc_stats_t *usb_serial_get_stats(void)
{
    return &s_pc_stats;
}

uint32_t usb_serial_get_last_data_time(void)
{
    return s_last_data_ms;
}

void usb_serial_register_handler(usb_cmd_handler_t handler)
{
    if (s_handler_count < MAX_CMD_HANDLERS && handler != NULL) {
        s_handlers[s_handler_count++] = handler;
    }
}

/* =============================================================================
 * SEND FUNCTIONS
 * ========================================================================== */

void usb_serial_send(const char *response)
{
    if (response) {
        size_t len = strlen(response);
        usb_serial_jtag_write_bytes((const uint8_t *)response, len, pdMS_TO_TICKS(100));
    }
}

void usb_serial_sendf(const char *fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len > 0) {
        usb_serial_jtag_write_bytes((const uint8_t *)buf, len, pdMS_TO_TICKS(100));
    }
}

/* =============================================================================
 * DATA PARSER
 * ========================================================================== */

static void parse_pc_data(const char *line)
{
    if (!line || strlen(line) < 5) return;

    /* Make a mutable copy for strtok */
    char buffer[USB_LINE_BUFFER_SIZE];
    strncpy(buffer, line, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    /* Parse into temporary struct first to avoid partial updates */
    pc_stats_t temp_stats = {0};
    int fields_parsed = 0;
    char *token = strtok(buffer, ",");

    while (token != NULL) {
        if (strncmp(token, "CPU:", 4) == 0) {
            temp_stats.cpu_percent = (int16_t)atoi(token + 4);
            fields_parsed++;
        }
        else if (strncmp(token, "CPUT:", 5) == 0) {
            temp_stats.cpu_temp = atof(token + 5);
            fields_parsed++;
        }
        else if (strncmp(token, "GPU:", 4) == 0) {
            temp_stats.gpu_percent = (int16_t)atoi(token + 4);
            fields_parsed++;
        }
        else if (strncmp(token, "GPUT:", 5) == 0) {
            temp_stats.gpu_temp = atof(token + 5);
            fields_parsed++;
        }
        else if (strncmp(token, "VRAM:", 5) == 0) {
            if (sscanf(token + 5, "%f/%f", &temp_stats.gpu_vram_used, &temp_stats.gpu_vram_total) == 2) {
                fields_parsed++;
            } else {
                temp_stats.gpu_vram_used = -1.0f;   /* N/A - do not invent values */
                temp_stats.gpu_vram_total = -1.0f;
            }
        }
        else if (strncmp(token, "RAM:", 4) == 0) {
            if (sscanf(token + 4, "%f/%f", &temp_stats.ram_used_gb, &temp_stats.ram_total_gb) == 2) {
                fields_parsed++;
            } else {
                temp_stats.ram_used_gb = -1.0f;     /* N/A - do not invent values */
                temp_stats.ram_total_gb = -1.0f;
            }
        }
        else if (strncmp(token, "NET:", 4) == 0) {
            strncpy(temp_stats.net_type, token + 4, sizeof(temp_stats.net_type) - 1);
            temp_stats.net_type[sizeof(temp_stats.net_type) - 1] = '\0';
            fields_parsed++;
        }
        else if (strncmp(token, "SPEED:", 6) == 0) {
            strncpy(temp_stats.net_speed, token + 6, sizeof(temp_stats.net_speed) - 1);
            temp_stats.net_speed[sizeof(temp_stats.net_speed) - 1] = '\0';
            fields_parsed++;
        }
        else if (strncmp(token, "DOWN:", 5) == 0) {
            temp_stats.net_down_mbps = atof(token + 5);
            fields_parsed++;
        }
        else if (strncmp(token, "UP:", 3) == 0) {
            temp_stats.net_up_mbps = atof(token + 3);
            fields_parsed++;
        }

        token = strtok(NULL, ",");
    }

    /* Only commit if we got enough fields (avoid partial/corrupt updates) */
    if (fields_parsed >= 5) {
        /* Thread-safe write with timeout - NEVER use portMAX_DELAY! */
        if (xSemaphoreTake(s_stats_mutex, pdMS_TO_TICKS(STATS_MUTEX_TIMEOUT_MS)) == pdTRUE) {
            /* Absorb single-sample sensor glitches: replace freshly-arrived
             * N/A fields with the last valid value for a few packets. */
            HOLD_FIELD(temp_stats.cpu_percent, s_pc_stats.cpu_percent, s_hold.cpu_pct);
            HOLD_FIELD(temp_stats.cpu_temp,    s_pc_stats.cpu_temp,    s_hold.cpu_temp);
            HOLD_FIELD(temp_stats.gpu_percent, s_pc_stats.gpu_percent, s_hold.gpu_pct);
            HOLD_FIELD(temp_stats.gpu_temp,    s_pc_stats.gpu_temp,    s_hold.gpu_temp);
            HOLD_FIELD(temp_stats.net_down_mbps, s_pc_stats.net_down_mbps, s_hold.net_dn);
            HOLD_FIELD(temp_stats.net_up_mbps,   s_pc_stats.net_up_mbps,   s_hold.net_up);

            /* VRAM and RAM are used/total pairs - hold both together */
            if (temp_stats.gpu_vram_used < 0) {
                if (s_hold.vram < HOLD_MAX_PACKETS) {
                    temp_stats.gpu_vram_used  = s_pc_stats.gpu_vram_used;
                    temp_stats.gpu_vram_total = s_pc_stats.gpu_vram_total;
                    s_hold.vram++;
                }
            } else {
                s_hold.vram = 0;
            }
            if (temp_stats.ram_used_gb < 0) {
                if (s_hold.ram < HOLD_MAX_PACKETS) {
                    temp_stats.ram_used_gb  = s_pc_stats.ram_used_gb;
                    temp_stats.ram_total_gb = s_pc_stats.ram_total_gb;
                    s_hold.ram++;
                }
            } else {
                s_hold.ram = 0;
            }

            s_pc_stats = temp_stats;
            s_last_data_ms = (uint32_t)(esp_timer_get_time() / 1000);
            xSemaphoreGive(s_stats_mutex);
            ESP_LOGD(TAG, "Parsed %d fields, timestamp updated", fields_parsed);
        } else {
            /* Fail-safe: Skip this update, don't freeze! */
            s_stats_mutex_timeouts++;
            ESP_LOGW(TAG, "Stats mutex timeout! Skipping data update. [timeouts: %lu]",
                     (unsigned long)s_stats_mutex_timeouts);
        }
    } else {
        ESP_LOGW(TAG, "Incomplete data: only %d fields parsed, discarding", fields_parsed);
    }
}

/* =============================================================================
 * HANDSHAKE HANDLER (built-in)
 * ========================================================================== */

static bool handle_handshake(const char *line)
{
    if (strcmp(line, "WHO_ARE_YOU?") == 0) {
        hw_identity_t *id = hw_identity_get();
        const esp_app_desc_t *app = esp_app_get_description();
        char response[128];
        /* |V:<version> and |N:<device name> appended in v2.4 -
         * client tolerates their absence (old FW). |N: may be empty. */
        snprintf(response, sizeof(response), "SCARAB_CLIENT_OK|H:%s|V:%s|N:%s\n",
                 id->identity_hash, app->version, id->device_name);
        usb_serial_send(response);
        ESP_LOGI(TAG, "Handshake: WHO_ARE_YOU? -> %s", response);
        return true;
    }
    return false;
}

/* =============================================================================
 * RX TASK
 * ========================================================================== */

static void usb_rx_task(void *arg)
{
    static uint8_t rx_buf[256];
    static char line_buf[USB_LINE_BUFFER_SIZE];
    static int line_pos = 0;
    static bool is_discarding = false;

    ESP_LOGI(TAG, "USB RX Task started");

    /* Subscribe to Task Watchdog - will reset ESP32 if task hangs */
    esp_task_wdt_add(NULL);

    while (1) {
        /* Feed the watchdog at start of each iteration */
        esp_task_wdt_reset();
        /* Read with timeout so other tasks can run */
        int len = usb_serial_jtag_read_bytes(rx_buf, sizeof(rx_buf), pdMS_TO_TICKS(10));

        if (len > 0) {
            for (int i = 0; i < len; i++) {
                uint8_t c = rx_buf[i];

                /* End of line - process it or end discard mode */
                if (c == '\n' || c == '\r') {
                    if (is_discarding) {
                        is_discarding = false;
                        line_pos = 0;
                    } else if (line_pos > 0) {
                        line_buf[line_pos] = '\0';

                        /* Try built-in handshake first */
                        if (!handle_handshake(line_buf)) {
                            /* Try registered handlers */
                            bool handled = false;
                            for (int h = 0; h < s_handler_count && !handled; h++) {
                                handled = s_handlers[h](line_buf);
                            }

                            /* If no handler matched, try to parse as PC data */
                            if (!handled) {
                                parse_pc_data(line_buf);
                            }
                        }

                        line_pos = 0;
                    }
                }
                /* In discard mode - ignore until newline */
                else if (is_discarding) {
                    /* Do nothing */
                }
                /* Normal character - add to buffer */
                else {
                    if (line_pos < USB_LINE_BUFFER_SIZE - 1) {
                        line_buf[line_pos++] = (char)c;
                    } else {
                        ESP_LOGW(TAG, "Line buffer overflow, discarding rest of line");
                        is_discarding = true;
                    }
                }
            }

            /* Yield to other tasks after processing data (Watchdog friendly) */
            vTaskDelay(pdMS_TO_TICKS(1));
        } else {
            /* No data - longer delay to save CPU, still yields for Watchdog */
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        /* Extra yield point for Task Watchdog Timer (TWDT) */
        taskYIELD();
    }
}

/* Task configuration - matches main_lvgl.c defines */
#define STACK_SIZE_USB_RX   6144    /* Increased for safety */
#define PRIO_USB_RX         4       /* Highest priority */

void usb_serial_start_rx_task(SemaphoreHandle_t stats_mutex)
{
    s_stats_mutex = stats_mutex;
    s_last_data_ms = (uint32_t)(esp_timer_get_time() / 1000);

    /* Create USB RX task with hardened configuration */
    xTaskCreate(usb_rx_task, "usb_rx", STACK_SIZE_USB_RX, NULL, PRIO_USB_RX, NULL);
    ESP_LOGI(TAG, "USB RX Task created (stack: %d, prio: %d)", STACK_SIZE_USB_RX, PRIO_USB_RX);
}
