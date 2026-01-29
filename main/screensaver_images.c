/**
 * @file screensaver_images.c
 * @brief Screensaver Image Management Implementation
 *
 * Phase 1.6 - Custom Image Upload Support
 * - Loads images from LittleFS (PSRAM)
 * - Falls back to compiled images if not found
 * - Supports chunked upload via serial protocol
 */

#include "screensaver_images.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "driver/usb_serial_jtag.h"

static const char *TAG = "SS-IMAGES";

/* =============================================================================
 * EXTERN: Compiled fallback images (from main_lvgl.c includes)
 * ========================================================================== */
extern const lv_image_dsc_t Sonic_Ring;
extern const lv_image_dsc_t triforce;
extern const lv_image_dsc_t DK_Fass;
extern const lv_image_dsc_t PacMan;

/* Fallback image table */
static const lv_image_dsc_t *fallback_images[SS_IMG_COUNT] = {
    &Sonic_Ring,     /* CPU - Sonic Ring */
    &triforce,       /* GPU - Triforce */
    &DK_Fass,        /* RAM - DK Barrel */
    &PacMan          /* NET - PacMan */
};

/* LittleFS paths table */
static const char *image_paths[SS_IMG_COUNT] = {
    SS_IMG_PATH_CPU,
    SS_IMG_PATH_GPU,
    SS_IMG_PATH_RAM,
    SS_IMG_PATH_NET
};

/* =============================================================================
 * GLOBAL STATE
 * ========================================================================== */

/* Loaded images (one per slot) */
static ss_loaded_image_t loaded_images[SS_IMG_COUNT] = {0};

/* Upload context */
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

    /* Clear all loaded images */
    memset(loaded_images, 0, sizeof(loaded_images));
    memset(&upload_ctx, 0, sizeof(upload_ctx));

    /* Check PSRAM availability */
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    ESP_LOGI(TAG, "PSRAM free: %u KB", (unsigned)(psram_free / 1024));

    /* Try to load existing images from LittleFS */
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

    /* Unload any existing image first */
    ss_image_unload(slot);

    const char *path = image_paths[slot];
    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGD(TAG, "No custom image at %s", path);
        return false;
    }

    /* Read header */
    scarab_img_header_t header;
    if (fread(&header, 1, sizeof(header), f) != sizeof(header)) {
        ESP_LOGE(TAG, "Failed to read header from %s", path);
        fclose(f);
        return false;
    }

    /* Validate header */
    if (header.magic != SCARAB_IMG_MAGIC) {
        ESP_LOGE(TAG, "Invalid magic in %s: 0x%08X", path, (unsigned)header.magic);
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

    /* Allocate PSRAM for pixel data */
    uint8_t *data = heap_caps_malloc(header.data_size, MALLOC_CAP_SPIRAM);
    if (!data) {
        ESP_LOGE(TAG, "Failed to allocate %u bytes PSRAM for %s",
                 (unsigned)header.data_size, path);
        fclose(f);
        return false;
    }

    /* Read pixel data */
    if (fread(data, 1, header.data_size, f) != header.data_size) {
        ESP_LOGE(TAG, "Failed to read pixel data from %s", path);
        heap_caps_free(data);
        fclose(f);
        return false;
    }

    fclose(f);

    /* Store in loaded images */
    ss_loaded_image_t *img = &loaded_images[slot];
    img->loaded = true;
    img->header = header;
    img->data = data;

    /* Setup LVGL image descriptor */
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

    ESP_LOGI(TAG, "Loaded %s: %dx%d, format=%d, size=%u",
             path, header.width, header.height, header.format,
             (unsigned)header.data_size);

    return true;
}

/* =============================================================================
 * GET LVGL IMAGE DESCRIPTOR
 * ========================================================================== */
const lv_image_dsc_t *ss_image_get_dsc(ss_image_slot_t slot)
{
    if (slot >= SS_IMG_COUNT) return NULL;

    /* Return custom image if loaded */
    if (loaded_images[slot].loaded && loaded_images[slot].data) {
        return &loaded_images[slot].lvgl_dsc;
    }

    /* Return compiled fallback */
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

    /* Validate header in data */
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
        ESP_LOGE(TAG, "Failed to write %s: wrote %u of %u",
                 path, (unsigned)written, (unsigned)size);
        return false;
    }

    ESP_LOGI(TAG, "Saved image to %s (%u bytes)", path, (unsigned)size);
    return true;
}

/* =============================================================================
 * DELETE IMAGE FROM LITTLEFS
 * ========================================================================== */
bool ss_image_delete(ss_image_slot_t slot)
{
    if (slot >= SS_IMG_COUNT) return false;

    /* Unload from PSRAM first */
    ss_image_unload(slot);

    const char *path = image_paths[slot];
    if (remove(path) == 0) {
        ESP_LOGI(TAG, "Deleted %s", path);
        return true;
    }

    /* File might not exist, that's OK */
    ESP_LOGD(TAG, "Could not delete %s (may not exist)", path);
    return true;
}

/* =============================================================================
 * UPLOAD PROTOCOL: IMG_BEGIN
 * Format: IMG_BEGIN:slot:size
 * ========================================================================== */
bool handle_img_begin(const char *line)
{
    /* Parse: IMG_BEGIN:slot:size */
    int slot;
    uint32_t size;

    if (sscanf(line, "IMG_BEGIN:%d:%u", &slot, &size) != 2) {
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

    /* Cancel any existing upload */
    if (upload_ctx.buffer) {
        heap_caps_free(upload_ctx.buffer);
    }

    /* Allocate receive buffer in PSRAM */
    upload_ctx.buffer = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (!upload_ctx.buffer) {
        send_response("IMG_ERR:NOMEM\n");
        return true;
    }

    upload_ctx.state = IMG_UPLOAD_RECEIVING;
    upload_ctx.slot = (ss_image_slot_t)slot;
    upload_ctx.expected_size = size;
    upload_ctx.received_size = 0;
    upload_ctx.crc32 = 0;

    ESP_LOGI(TAG, "Upload started: slot=%d, size=%u", slot, (unsigned)size);
    send_response("IMG_OK:BEGIN\n");

    return true;
}

/* =============================================================================
 * UPLOAD PROTOCOL: IMG_DATA
 * Format: IMG_DATA:offset:hexdata
 * ========================================================================== */
bool handle_img_data(const char *line)
{
    if (upload_ctx.state != IMG_UPLOAD_RECEIVING) {
        send_response("IMG_ERR:NOBEGIN\n");
        return true;
    }

    /* Parse offset */
    uint32_t offset;
    const char *hex_start = NULL;

    if (sscanf(line, "IMG_DATA:%u:", &offset) != 1) {
        send_response("IMG_ERR:PARSE\n");
        return true;
    }

    /* Find hex data start (after second colon) */
    hex_start = strchr(line + 9, ':');  /* Skip "IMG_DATA:" */
    if (!hex_start) {
        send_response("IMG_ERR:PARSE\n");
        return true;
    }
    hex_start++;  /* Skip the colon */

    /* Validate offset */
    if (offset != upload_ctx.received_size) {
        send_response("IMG_ERR:OFFSET:%u\n", (unsigned)upload_ctx.received_size);
        return true;
    }

    /* Decode hex data */
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

    /* Convert hex to binary */
    uint8_t *dest = upload_ctx.buffer + upload_ctx.received_size;
    for (size_t i = 0; i < data_len; i++) {
        char byte_str[3] = { hex_start[i*2], hex_start[i*2+1], 0 };
        dest[i] = (uint8_t)strtoul(byte_str, NULL, 16);
    }

    /* Update CRC */
    upload_ctx.crc32 = crc32_update(upload_ctx.crc32, dest, data_len);
    upload_ctx.received_size += data_len;

    /* Progress feedback every 10KB */
    if (upload_ctx.received_size % 10240 < data_len) {
        ESP_LOGI(TAG, "Upload progress: %u / %u bytes",
                 (unsigned)upload_ctx.received_size,
                 (unsigned)upload_ctx.expected_size);
    }

    send_response("IMG_OK:DATA:%u\n", (unsigned)upload_ctx.received_size);
    return true;
}

/* =============================================================================
 * UPLOAD PROTOCOL: IMG_END
 * Format: IMG_END:crc32
 * ========================================================================== */
bool handle_img_end(const char *line)
{
    if (upload_ctx.state != IMG_UPLOAD_RECEIVING) {
        send_response("IMG_ERR:NOBEGIN\n");
        return true;
    }

    /* Parse expected CRC */
    uint32_t expected_crc;
    if (sscanf(line, "IMG_END:%x", &expected_crc) != 1) {
        send_response("IMG_ERR:PARSE\n");
        return true;
    }

    /* Check size */
    if (upload_ctx.received_size != upload_ctx.expected_size) {
        send_response("IMG_ERR:INCOMPLETE:%u\n", (unsigned)upload_ctx.received_size);
        return true;
    }

    /* Verify CRC */
    if (upload_ctx.crc32 != expected_crc) {
        ESP_LOGE(TAG, "CRC mismatch: got 0x%08X, expected 0x%08X",
                 (unsigned)upload_ctx.crc32, (unsigned)expected_crc);
        send_response("IMG_ERR:CRC:%08X\n", (unsigned)upload_ctx.crc32);

        /* Cleanup */
        heap_caps_free(upload_ctx.buffer);
        upload_ctx.buffer = NULL;
        upload_ctx.state = IMG_UPLOAD_IDLE;
        return true;
    }

    /* Validate header in buffer */
    const scarab_img_header_t *header = (const scarab_img_header_t *)upload_ctx.buffer;
    if (header->magic != SCARAB_IMG_MAGIC) {
        send_response("IMG_ERR:MAGIC\n");
        heap_caps_free(upload_ctx.buffer);
        upload_ctx.buffer = NULL;
        upload_ctx.state = IMG_UPLOAD_IDLE;
        return true;
    }

    /* Save to LittleFS */
    if (!ss_image_save(upload_ctx.slot, upload_ctx.buffer, upload_ctx.received_size)) {
        send_response("IMG_ERR:SAVE\n");
        heap_caps_free(upload_ctx.buffer);
        upload_ctx.buffer = NULL;
        upload_ctx.state = IMG_UPLOAD_IDLE;
        return true;
    }

    /* Reload the image (this will free and reallocate) */
    heap_caps_free(upload_ctx.buffer);
    upload_ctx.buffer = NULL;
    upload_ctx.state = IMG_UPLOAD_COMPLETE;

    if (ss_image_load(upload_ctx.slot)) {
        ESP_LOGI(TAG, "Upload complete: slot=%d, CRC=0x%08X",
                 upload_ctx.slot, (unsigned)upload_ctx.crc32);
        send_response("IMG_OK:COMPLETE:%d\n", upload_ctx.slot);
    } else {
        send_response("IMG_ERR:LOAD\n");
    }

    upload_ctx.state = IMG_UPLOAD_IDLE;
    return true;
}

/* =============================================================================
 * UPLOAD PROTOCOL: IMG_ABORT
 * ========================================================================== */
bool handle_img_abort(void)
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

/* =============================================================================
 * UPLOAD PROTOCOL: IMG_DELETE
 * Format: IMG_DELETE:slot
 * ========================================================================== */
bool handle_img_delete(const char *line)
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

/* =============================================================================
 * UPLOAD PROTOCOL: IMG_STATUS
 * ========================================================================== */
bool handle_img_status(void)
{
    /* Report upload state */
    send_response("IMG_STATUS:UPLOAD:%d:%u:%u\n",
                  upload_ctx.state,
                  (unsigned)upload_ctx.received_size,
                  (unsigned)upload_ctx.expected_size);

    /* Report loaded images */
    for (int i = 0; i < SS_IMG_COUNT; i++) {
        send_response("IMG_STATUS:SLOT:%d:%d:%u\n",
                      i,
                      loaded_images[i].loaded ? 1 : 0,
                      loaded_images[i].loaded ? (unsigned)loaded_images[i].header.data_size : 0);
    }

    return true;
}

/* =============================================================================
 * MAIN COMMAND DISPATCHER
 * ========================================================================== */
bool handle_image_command(const char *line)
{
    if (strncmp(line, "IMG_", 4) != 0) {
        return false;  /* Not an image command */
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
