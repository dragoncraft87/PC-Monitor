/**
 * @file fw_update.c
 * @brief Serial Firmware Update (OTA over USB) Implementation
 *
 * Runs entirely in the USB RX task. Uses OTA_WITH_SEQUENTIAL_WRITES so flash
 * sectors are erased lazily during writes - no long blocking erase that would
 * trip the 5s Task Watchdog.
 */

#include "fw_update.h"
#include "usb_serial_comm.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_app_desc.h"
#include "esp_system.h"

static const char *TAG = "FW-UPDATE";

/* Chunk payload limit: FW_DATA line = "FW_DATA:<offset>:" + 2*N hex chars.
 * With N=1024 the line is ~2065 chars, well inside USB_LINE_BUFFER_SIZE (4096). */
#define FW_CHUNK_MAX        1024

/* Minimum plausible app image size (real builds are ~1MB+) */
#define FW_MIN_SIZE         0x10000

typedef enum {
    FW_STATE_IDLE = 0,
    FW_STATE_RECEIVING
} fw_state_t;

typedef struct {
    fw_state_t state;
    const esp_partition_t *target;
    esp_ota_handle_t ota_handle;
    uint32_t expected_size;
    uint32_t received_size;
    uint32_t crc32;
} fw_ctx_t;

static fw_ctx_t s_ctx = {0};

/* =============================================================================
 * CRC32 (same polynomial/convention as image upload and C# client)
 * ========================================================================== */
#define CRC32_INIT 0xFFFFFFFF

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len)
{
    while (len--) {
        crc ^= *data++;
        for (int i = 0; i < 8; i++) {
            crc = (crc >> 1) ^ (0xEDB88320 & ~((crc & 1) - 1));
        }
    }
    return crc;
}

/* =============================================================================
 * HELPERS
 * ========================================================================== */

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

/* Decode hex string into buf. Returns decoded byte count, or -1 on bad input. */
static int hex_decode(const char *hex, uint8_t *buf, size_t buf_size)
{
    size_t hex_len = strlen(hex);
    if (hex_len % 2 != 0 || hex_len / 2 > buf_size) return -1;

    for (size_t i = 0; i < hex_len / 2; i++) {
        int hi = hex_nibble(hex[i * 2]);
        int lo = hex_nibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return -1;
        buf[i] = (uint8_t)((hi << 4) | lo);
    }
    return (int)(hex_len / 2);
}

static void fw_abort_upload(void)
{
    if (s_ctx.state == FW_STATE_RECEIVING) {
        esp_ota_abort(s_ctx.ota_handle);
    }
    memset(&s_ctx, 0, sizeof(s_ctx));
}

/* =============================================================================
 * COMMAND HANDLERS
 * ========================================================================== */

static bool handle_fw_begin(const char *line)
{
    unsigned long size;
    if (sscanf(line, "FW_BEGIN:%lu", &size) != 1) {
        usb_serial_send("FW_ERR:PARSE\n");
        return true;
    }

    /* Cancel any previous unfinished upload */
    fw_abort_upload();

    const esp_partition_t *target = esp_ota_get_next_update_partition(NULL);
    if (!target) {
        ESP_LOGE(TAG, "No OTA update partition found (partition table has no ota_0/ota_1?)");
        usb_serial_send("FW_ERR:NOPART\n");
        return true;
    }

    if (size < FW_MIN_SIZE || size > target->size) {
        ESP_LOGE(TAG, "Invalid size %lu (partition %s is %" PRIu32 " bytes)",
                 size, target->label, target->size);
        usb_serial_send("FW_ERR:SIZE\n");
        return true;
    }

    /* OTA_WITH_SEQUENTIAL_WRITES: erase lazily per sector during writes,
     * keeps each FW_DATA handler call short (watchdog-safe). */
    esp_err_t err = esp_ota_begin(target, OTA_WITH_SEQUENTIAL_WRITES, &s_ctx.ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        usb_serial_send("FW_ERR:OTABEGIN\n");
        return true;
    }

    s_ctx.state = FW_STATE_RECEIVING;
    s_ctx.target = target;
    s_ctx.expected_size = (uint32_t)size;
    s_ctx.received_size = 0;
    s_ctx.crc32 = CRC32_INIT;

    ESP_LOGI(TAG, "FW update started: %lu bytes -> partition %s", size, target->label);
    usb_serial_send("FW_OK:BEGIN\n");
    return true;
}

static bool handle_fw_data(const char *line)
{
    if (s_ctx.state != FW_STATE_RECEIVING) {
        usb_serial_send("FW_ERR:NOBEGIN\n");
        return true;
    }

    unsigned long offset;
    if (sscanf(line, "FW_DATA:%lu:", &offset) != 1) {
        usb_serial_send("FW_ERR:PARSE\n");
        return true;
    }

    const char *hex_start = strchr(line + 8, ':');
    if (!hex_start) {
        usb_serial_send("FW_ERR:PARSE\n");
        return true;
    }
    hex_start++;

    /* Sequential offset check with resync info: the client parses the expected
     * offset from this response and rewinds/skips accordingly (lost-ACK safe). */
    if ((uint32_t)offset != s_ctx.received_size) {
        usb_serial_sendf("FW_ERR:OFFSET:%" PRIu32 "\n", s_ctx.received_size);
        return true;
    }

    static uint8_t chunk_buf[FW_CHUNK_MAX];
    int data_len = hex_decode(hex_start, chunk_buf, sizeof(chunk_buf));
    if (data_len <= 0) {
        usb_serial_send("FW_ERR:HEX\n");
        return true;
    }

    if (s_ctx.received_size + (uint32_t)data_len > s_ctx.expected_size) {
        usb_serial_send("FW_ERR:OVERFLOW\n");
        return true;
    }

    esp_err_t err = esp_ota_write(s_ctx.ota_handle, chunk_buf, (size_t)data_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_write failed at %" PRIu32 ": %s",
                 s_ctx.received_size, esp_err_to_name(err));
        fw_abort_upload();
        usb_serial_send("FW_ERR:WRITE\n");
        return true;
    }

    s_ctx.crc32 = crc32_update(s_ctx.crc32, chunk_buf, (size_t)data_len);
    s_ctx.received_size += (uint32_t)data_len;

    if (s_ctx.received_size % 65536 < (uint32_t)data_len) {
        ESP_LOGI(TAG, "FW progress: %" PRIu32 " / %" PRIu32 " bytes",
                 s_ctx.received_size, s_ctx.expected_size);
    }

    usb_serial_sendf("FW_OK:DATA:%" PRIu32 "\n", s_ctx.received_size);
    return true;
}

static bool handle_fw_end(const char *line)
{
    if (s_ctx.state != FW_STATE_RECEIVING) {
        usb_serial_send("FW_ERR:NOBEGIN\n");
        return true;
    }

    unsigned int expected_crc;
    if (sscanf(line, "FW_END:%x", &expected_crc) != 1) {
        usb_serial_send("FW_ERR:PARSE\n");
        return true;
    }

    if (s_ctx.received_size != s_ctx.expected_size) {
        usb_serial_sendf("FW_ERR:INCOMPLETE:%" PRIu32 "\n", s_ctx.received_size);
        return true;
    }

    uint32_t final_crc = ~s_ctx.crc32;
    if (final_crc != (uint32_t)expected_crc) {
        ESP_LOGE(TAG, "FW CRC mismatch: got 0x%08" PRIX32 ", expected 0x%08X",
                 final_crc, expected_crc);
        fw_abort_upload();
        usb_serial_sendf("FW_ERR:CRC:%08" PRIX32 "\n", final_crc);
        return true;
    }

    /* esp_ota_end validates the image (magic byte, SHA256, segments) */
    esp_err_t err = esp_ota_end(s_ctx.ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        memset(&s_ctx, 0, sizeof(s_ctx));
        usb_serial_send("FW_ERR:VALIDATE\n");
        return true;
    }

    err = esp_ota_set_boot_partition(s_ctx.target);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        memset(&s_ctx, 0, sizeof(s_ctx));
        usb_serial_send("FW_ERR:SETBOOT\n");
        return true;
    }

    ESP_LOGI(TAG, "FW update verified, booting into %s. Restarting...", s_ctx.target->label);
    usb_serial_send("FW_OK:COMPLETE\n");
    memset(&s_ctx, 0, sizeof(s_ctx));

    /* Give the USB driver time to flush the response before rebooting */
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return true; /* not reached */
}

static bool handle_fw_version(void)
{
    const esp_app_desc_t *desc = esp_app_get_description();
    const esp_partition_t *running = esp_ota_get_running_partition();
    usb_serial_sendf("FW_VER:%s:%s\n",
                     desc->version,
                     running ? running->label : "unknown");
    return true;
}

/* =============================================================================
 * MAIN COMMAND DISPATCHER
 * ========================================================================== */
bool fw_update_handle_command(const char *line)
{
    if (strcmp(line, "GET_FW_VER") == 0) {
        return handle_fw_version();
    }

    if (strncmp(line, "FW_", 3) != 0) {
        return false;
    }

    if (strncmp(line, "FW_BEGIN:", 9) == 0) {
        return handle_fw_begin(line);
    }
    else if (strncmp(line, "FW_DATA:", 8) == 0) {
        return handle_fw_data(line);
    }
    else if (strncmp(line, "FW_END:", 7) == 0) {
        return handle_fw_end(line);
    }
    else if (strcmp(line, "FW_ABORT") == 0) {
        fw_abort_upload();
        ESP_LOGI(TAG, "FW update aborted");
        usb_serial_send("FW_OK:ABORT\n");
        return true;
    }

    return false;
}
