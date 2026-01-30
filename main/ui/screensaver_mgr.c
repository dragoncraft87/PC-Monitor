/**
 * @file screensaver_mgr.c
 * @brief Screensaver Image Management Implementation
 */

#include "screensaver_mgr.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "driver/usb_serial_jtag.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SS-MGR";

/* =============================================================================
 * EXTERN: Compiled system icons (Desert-Spec v2.2)
 * ========================================================================== */
extern const lv_image_dsc_t CPU;
extern const lv_image_dsc_t GPU;
extern const lv_image_dsc_t RAM;
extern const lv_image_dsc_t NET;

/* Fallback image table - maps to slot indices */
static const lv_image_dsc_t *fallback_images[SS_IMG_COUNT] = {
    &CPU,   /* SS_IMG_CPU = 0 */
    &GPU,   /* SS_IMG_GPU = 1 */
    &RAM,   /* SS_IMG_RAM = 2 */
    &NET    /* SS_IMG_NET = 3 */
};

/* LittleFS paths table */
static const char *image_paths[SS_IMG_COUNT] = {
    SS_IMG_PATH_CPU,
    SS_IMG_PATH_GPU,
    SS_IMG_PATH_RAM,
    SS_IMG_PATH_NET
};

/* =============================================================================
 * INTERNAL STRUCTURES
 * ========================================================================== */

typedef struct {
    bool loaded;
    scarab_img_header_t header;
    uint8_t *data;
    lv_image_dsc_t lvgl_dsc;
} ss_loaded_image_t;

typedef enum {
    IMG_UPLOAD_IDLE = 0,
    IMG_UPLOAD_RECEIVING,
    IMG_UPLOAD_COMPLETE,
    IMG_UPLOAD_ERROR
} img_upload_state_t;

typedef struct {
    img_upload_state_t state;
    ss_image_slot_t slot;
    uint32_t expected_size;
    uint32_t received_size;
    uint8_t *buffer;
    uint32_t crc32;
} img_upload_ctx_t;

/* Global state */
static ss_loaded_image_t loaded_images[SS_IMG_COUNT] = {0};
static img_upload_ctx_t upload_ctx = {0};

/* =============================================================================
 * CRC32 CALCULATION
 * ========================================================================== */
static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len)
{
    crc = ~crc;
    while (len--) {
        crc ^= *data++;
        for (int i = 0; i < 8; i++) {
            crc = (crc >> 1) ^ (0xEDB88320 & ~((crc & 1) - 1));
        }
    }
    return ~crc;
}

/* =============================================================================
 * HELPER: Send response via USB Serial
 * ========================================================================== */
static void send_response(const char *fmt, ...)
{
    char buf[128];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len > 0) {
        usb_serial_jtag_write_bytes((const uint8_t *)buf, len, pdMS_TO_TICKS(100));
    }
}

/* =============================================================================
 * INITIALIZE IMAGE SYSTEM
 * ========================================================================== */
void ss_images_init(void)
{
    ESP_LOGI(TAG, "Initializing screensaver image system...");

    memset(loaded_images, 0, sizeof(loaded_images));
    memset(&upload_ctx, 0, sizeof(upload_ctx));

    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    ESP_LOGI(TAG, "PSRAM free: %lu KB", (unsigned long)(psram_free / 1024));

    for (int i = 0; i < SS_IMG_COUNT; i++) {
        if (ss_image_load((ss_image_slot_t)i)) {
            ESP_LOGI(TAG, "Slot %d: Loaded custom image from LFS", i);
        } else {
            ESP_LOGI(TAG, "Slot %d: Using compiled fallback", i);
        }
    }
}

/* =============================================================================
 * LOAD IMAGE FROM LITTLEFS
 * ========================================================================== */
bool ss_image_load(ss_image_slot_t slot)
{
    if (slot >= SS_IMG_COUNT) return false;

    ss_image_unload(slot);

    const char *path = image_paths[slot];
    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGD(TAG, "No custom image at %s", path);
        return false;
    }

    scarab_img_header_t header;
    if (fread(&header, 1, sizeof(header), f) != sizeof(header)) {
        ESP_LOGE(TAG, "Failed to read header from %s", path);
        fclose(f);
        return false;
    }

    if (header.magic != SCARAB_IMG_MAGIC) {
        ESP_LOGE(TAG, "Invalid magic in %s: 0x%08" PRIX32, path, header.magic);
        fclose(f);
        return false;
    }

    if (header.width != SCARAB_IMG_WIDTH || header.height != SCARAB_IMG_HEIGHT) {
        ESP_LOGE(TAG, "Invalid dimensions in %s: %dx%d", path, header.width, header.height);
        fclose(f);
        return false;
    }

    if (header.format != SCARAB_FMT_RGB565 && header.format != SCARAB_FMT_RGB565A8) {
        ESP_LOGE(TAG, "Invalid format in %s: %d", path, header.format);
        fclose(f);
        return false;
    }

    uint8_t *data = heap_caps_malloc(header.data_size, MALLOC_CAP_SPIRAM);
    if (!data) {
        ESP_LOGE(TAG, "Failed to allocate %" PRIu32 " bytes PSRAM for %s", header.data_size, path);
        fclose(f);
        return false;
    }

    if (fread(data, 1, header.data_size, f) != header.data_size) {
        ESP_LOGE(TAG, "Failed to read pixel data from %s", path);
        heap_caps_free(data);
        fclose(f);
        return false;
    }

    fclose(f);

    ss_loaded_image_t *img = &loaded_images[slot];
    img->loaded = true;
    img->header = header;
    img->data = data;

    img->lvgl_dsc.header.w = header.width;
    img->lvgl_dsc.header.h = header.height;
    img->lvgl_dsc.header.stride = (header.format == SCARAB_FMT_RGB565A8)
                                   ? header.width * 3
                                   : header.width * 2;
    img->lvgl_dsc.header.cf = (header.format == SCARAB_FMT_RGB565A8)
                               ? LV_COLOR_FORMAT_RGB565A8
                               : LV_COLOR_FORMAT_RGB565;
    img->lvgl_dsc.data = data;
    img->lvgl_dsc.data_size = header.data_size;

    ESP_LOGI(TAG, "Loaded %s: %dx%d, format=%d, size=%" PRIu32,
             path, header.width, header.height, header.format, header.data_size);

    return true;
}

/* =============================================================================
 * GET LVGL IMAGE DESCRIPTOR
 * ========================================================================== */
const lv_image_dsc_t *ss_image_get_dsc(ss_image_slot_t slot)
{
    if (slot >= SS_IMG_COUNT) return NULL;

    if (loaded_images[slot].loaded && loaded_images[slot].data) {
        return &loaded_images[slot].lvgl_dsc;
    }

    return fallback_images[slot];
}

/* =============================================================================
 * CHECK IF CUSTOM IMAGE
 * ========================================================================== */
bool ss_image_is_custom(ss_image_slot_t slot)
{
    if (slot >= SS_IMG_COUNT) return false;
    return loaded_images[slot].loaded;
}

/* =============================================================================
 * UNLOAD IMAGE FROM PSRAM
 * ========================================================================== */
void ss_image_unload(ss_image_slot_t slot)
{
    if (slot >= SS_IMG_COUNT) return;

    ss_loaded_image_t *img = &loaded_images[slot];
    if (img->data) {
        heap_caps_free(img->data);
        img->data = NULL;
    }
    img->loaded = false;
    memset(&img->header, 0, sizeof(img->header));
    memset(&img->lvgl_dsc, 0, sizeof(img->lvgl_dsc));
}

/* =============================================================================
 * SAVE IMAGE TO LITTLEFS
 * ========================================================================== */
bool ss_image_save(ss_image_slot_t slot, const uint8_t *data, uint32_t size)
{
    if (slot >= SS_IMG_COUNT || !data || size < SCARAB_IMG_HEADER_SIZE) {
        return false;
    }

    const scarab_img_header_t *header = (const scarab_img_header_t *)data;
    if (header->magic != SCARAB_IMG_MAGIC) {
        ESP_LOGE(TAG, "Invalid magic in upload data");
        return false;
    }

    const char *path = image_paths[slot];
    FILE *f = fopen(path, "wb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open %s for writing", path);
        return false;
    }

    size_t written = fwrite(data, 1, size, f);
    fclose(f);

    if (written != size) {
        ESP_LOGE(TAG, "Failed to write %s: wrote %zu of %" PRIu32, path, written, size);
        return false;
    }

    ESP_LOGI(TAG, "Saved image to %s (%" PRIu32 " bytes)", path, size);
    return true;
}

/* =============================================================================
 * DELETE IMAGE FROM LITTLEFS
 * ========================================================================== */
bool ss_image_delete(ss_image_slot_t slot)
{
    if (slot >= SS_IMG_COUNT) return false;

    ss_image_unload(slot);

    const char *path = image_paths[slot];
    if (remove(path) == 0) {
        ESP_LOGI(TAG, "Deleted %s", path);
        return true;
    }

    ESP_LOGD(TAG, "Could not delete %s (may not exist)", path);
    return true;
}

/* =============================================================================
 * UPLOAD PROTOCOL HANDLERS
 * ========================================================================== */

static bool handle_img_begin(const char *line)
{
    int slot;
    unsigned long size;

    if (sscanf(line, "IMG_BEGIN:%d:%lu", &slot, &size) != 2) {
        send_response("IMG_ERR:PARSE\n");
        return true;
    }

    if (slot < 0 || slot >= SS_IMG_COUNT) {
        send_response("IMG_ERR:SLOT\n");
        return true;
    }

    if (size < SCARAB_IMG_HEADER_SIZE || size > SCARAB_IMG_MAX_SIZE) {
        send_response("IMG_ERR:SIZE\n");
        return true;
    }

    if (upload_ctx.buffer) {
        heap_caps_free(upload_ctx.buffer);
    }

    upload_ctx.buffer = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (!upload_ctx.buffer) {
        send_response("IMG_ERR:NOMEM\n");
        return true;
    }

    upload_ctx.state = IMG_UPLOAD_RECEIVING;
    upload_ctx.slot = (ss_image_slot_t)slot;
    upload_ctx.expected_size = (uint32_t)size;
    upload_ctx.received_size = 0;
    upload_ctx.crc32 = 0;

    ESP_LOGI(TAG, "Upload started: slot=%d, size=%lu", slot, size);
    send_response("IMG_OK:BEGIN\n");

    return true;
}

static bool handle_img_data(const char *line)
{
    if (upload_ctx.state != IMG_UPLOAD_RECEIVING) {
        send_response("IMG_ERR:NOBEGIN\n");
        return true;
    }

    unsigned long offset;
    if (sscanf(line, "IMG_DATA:%lu:", &offset) != 1) {
        send_response("IMG_ERR:PARSE\n");
        return true;
    }

    const char *hex_start = strchr(line + 9, ':');
    if (!hex_start) {
        send_response("IMG_ERR:PARSE\n");
        return true;
    }
    hex_start++;

    if ((uint32_t)offset != upload_ctx.received_size) {
        send_response("IMG_ERR:OFFSET:%" PRIu32 "\n", upload_ctx.received_size);
        return true;
    }

    size_t hex_len = strlen(hex_start);
    if (hex_len % 2 != 0) {
        send_response("IMG_ERR:HEXLEN\n");
        return true;
    }

    size_t data_len = hex_len / 2;
    if (upload_ctx.received_size + data_len > upload_ctx.expected_size) {
        send_response("IMG_ERR:OVERFLOW\n");
        return true;
    }

    uint8_t *dest = upload_ctx.buffer + upload_ctx.received_size;
    for (size_t i = 0; i < data_len; i++) {
        char byte_str[3] = { hex_start[i*2], hex_start[i*2+1], 0 };
        dest[i] = (uint8_t)strtoul(byte_str, NULL, 16);
    }

    upload_ctx.crc32 = crc32_update(upload_ctx.crc32, dest, data_len);
    upload_ctx.received_size += (uint32_t)data_len;

    if (upload_ctx.received_size % 10240 < data_len) {
        ESP_LOGI(TAG, "Upload progress: %" PRIu32 " / %" PRIu32 " bytes",
                 upload_ctx.received_size, upload_ctx.expected_size);
    }

    send_response("IMG_OK:DATA:%" PRIu32 "\n", upload_ctx.received_size);
    return true;
}

static bool handle_img_end(const char *line)
{
    if (upload_ctx.state != IMG_UPLOAD_RECEIVING) {
        send_response("IMG_ERR:NOBEGIN\n");
        return true;
    }

    unsigned int expected_crc;
    if (sscanf(line, "IMG_END:%x", &expected_crc) != 1) {
        send_response("IMG_ERR:PARSE\n");
        return true;
    }

    if (upload_ctx.received_size != upload_ctx.expected_size) {
        send_response("IMG_ERR:INCOMPLETE:%" PRIu32 "\n", upload_ctx.received_size);
        return true;
    }

    if (upload_ctx.crc32 != (uint32_t)expected_crc) {
        ESP_LOGE(TAG, "CRC mismatch: got 0x%08" PRIX32 ", expected 0x%08X",
                 upload_ctx.crc32, expected_crc);
        send_response("IMG_ERR:CRC:%08" PRIX32 "\n", upload_ctx.crc32);

        heap_caps_free(upload_ctx.buffer);
        upload_ctx.buffer = NULL;
        upload_ctx.state = IMG_UPLOAD_IDLE;
        return true;
    }

    const scarab_img_header_t *header = (const scarab_img_header_t *)upload_ctx.buffer;
    if (header->magic != SCARAB_IMG_MAGIC) {
        send_response("IMG_ERR:MAGIC\n");
        heap_caps_free(upload_ctx.buffer);
        upload_ctx.buffer = NULL;
        upload_ctx.state = IMG_UPLOAD_IDLE;
        return true;
    }

    if (!ss_image_save(upload_ctx.slot, upload_ctx.buffer, upload_ctx.received_size)) {
        send_response("IMG_ERR:SAVE\n");
        heap_caps_free(upload_ctx.buffer);
        upload_ctx.buffer = NULL;
        upload_ctx.state = IMG_UPLOAD_IDLE;
        return true;
    }

    heap_caps_free(upload_ctx.buffer);
    upload_ctx.buffer = NULL;
    upload_ctx.state = IMG_UPLOAD_COMPLETE;

    if (ss_image_load(upload_ctx.slot)) {
        ESP_LOGI(TAG, "Upload complete: slot=%d, CRC=0x%08" PRIX32,
                 upload_ctx.slot, upload_ctx.crc32);
        send_response("IMG_OK:COMPLETE:%d\n", upload_ctx.slot);
    } else {
        send_response("IMG_ERR:LOAD\n");
    }

    upload_ctx.state = IMG_UPLOAD_IDLE;
    return true;
}

static bool handle_img_abort(void)
{
    if (upload_ctx.buffer) {
        heap_caps_free(upload_ctx.buffer);
        upload_ctx.buffer = NULL;
    }
    upload_ctx.state = IMG_UPLOAD_IDLE;
    upload_ctx.received_size = 0;

    ESP_LOGI(TAG, "Upload aborted");
    send_response("IMG_OK:ABORT\n");
    return true;
}

static bool handle_img_delete(const char *line)
{
    int slot;
    if (sscanf(line, "IMG_DELETE:%d", &slot) != 1) {
        send_response("IMG_ERR:PARSE\n");
        return true;
    }

    if (slot < 0 || slot >= SS_IMG_COUNT) {
        send_response("IMG_ERR:SLOT\n");
        return true;
    }

    ss_image_delete((ss_image_slot_t)slot);
    send_response("IMG_OK:DELETE:%d\n", slot);
    return true;
}

static bool handle_img_status(void)
{
    send_response("IMG_STATUS:UPLOAD:%d:%" PRIu32 ":%" PRIu32 "\n",
                  upload_ctx.state,
                  upload_ctx.received_size,
                  upload_ctx.expected_size);

    for (int i = 0; i < SS_IMG_COUNT; i++) {
        send_response("IMG_STATUS:SLOT:%d:%d:%" PRIu32 "\n",
                      i,
                      loaded_images[i].loaded ? 1 : 0,
                      loaded_images[i].loaded ? loaded_images[i].header.data_size : 0);
    }

    return true;
}

/* =============================================================================
 * MAIN COMMAND DISPATCHER
 * ========================================================================== */
bool ss_image_handle_command(const char *line)
{
    if (strncmp(line, "IMG_", 4) != 0) {
        return false;
    }

    if (strncmp(line, "IMG_BEGIN:", 10) == 0) {
        return handle_img_begin(line);
    }
    else if (strncmp(line, "IMG_DATA:", 9) == 0) {
        return handle_img_data(line);
    }
    else if (strncmp(line, "IMG_END:", 8) == 0) {
        return handle_img_end(line);
    }
    else if (strcmp(line, "IMG_ABORT") == 0) {
        return handle_img_abort();
    }
    else if (strncmp(line, "IMG_DELETE:", 11) == 0) {
        return handle_img_delete(line);
    }
    else if (strcmp(line, "IMG_STATUS") == 0) {
        return handle_img_status();
    }

    return false;
}
