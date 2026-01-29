/**
 * @file screensaver_images.h
 * @brief Screensaver Image Management - LittleFS Storage
 *
 * Phase 1.6 - Custom Image Upload Support
 * Supports RGB565A8 format (16-bit color + 8-bit alpha) for transparency
 */

#ifndef SCREENSAVER_IMAGES_H
#define SCREENSAVER_IMAGES_H

#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"

/* =============================================================================
 * IMAGE FORMAT DEFINITIONS
 * ========================================================================== */

#define SCARAB_IMG_MAGIC        0x53434152  /* "SCAR" in little-endian */
#define SCARAB_IMG_VERSION      1

/* Image format types */
typedef enum {
    SCARAB_FMT_RGB565   = 0,    /* 16-bit color, no alpha (2 bytes/pixel) */
    SCARAB_FMT_RGB565A8 = 1,    /* 16-bit color + 8-bit alpha (3 bytes/pixel) */
} scarab_img_format_t;

/* Image file header (16 bytes, aligned) */
typedef struct __attribute__((packed)) {
    uint32_t magic;             /* Must be SCARAB_IMG_MAGIC */
    uint16_t width;             /* Image width (typically 240) */
    uint16_t height;            /* Image height (typically 240) */
    uint8_t  format;            /* scarab_img_format_t */
    uint8_t  version;           /* Header version */
    uint16_t reserved;          /* Padding for alignment */
    uint32_t data_size;         /* Size of pixel data in bytes */
} scarab_img_header_t;

/* Size calculations for 240x240 images */
#define SCARAB_IMG_WIDTH        240
#define SCARAB_IMG_HEIGHT       240
#define SCARAB_IMG_PIXELS       (SCARAB_IMG_WIDTH * SCARAB_IMG_HEIGHT)  /* 57600 */

#define SCARAB_RGB565_SIZE      (SCARAB_IMG_PIXELS * 2)     /* 115200 bytes */
#define SCARAB_RGB565A8_SIZE    (SCARAB_IMG_PIXELS * 3)     /* 172800 bytes */

#define SCARAB_IMG_HEADER_SIZE  sizeof(scarab_img_header_t) /* 16 bytes */
#define SCARAB_IMG_MAX_SIZE     (SCARAB_IMG_HEADER_SIZE + SCARAB_RGB565A8_SIZE)  /* ~173 KB */

/* =============================================================================
 * SCREENSAVER IMAGE SLOTS
 * ========================================================================== */

typedef enum {
    SS_IMG_CPU = 0,
    SS_IMG_GPU = 1,
    SS_IMG_RAM = 2,
    SS_IMG_NET = 3,
    SS_IMG_COUNT = 4
} ss_image_slot_t;

/* LittleFS paths for screensaver images */
#define SS_IMG_PATH_CPU     "/storage/ss_cpu.bin"
#define SS_IMG_PATH_GPU     "/storage/ss_gpu.bin"
#define SS_IMG_PATH_RAM     "/storage/ss_ram.bin"
#define SS_IMG_PATH_NET     "/storage/ss_net.bin"

/* =============================================================================
 * LOADED IMAGE STRUCTURE (in PSRAM)
 * ========================================================================== */

typedef struct {
    bool loaded;                    /* True if image loaded from LFS */
    scarab_img_header_t header;     /* Image header */
    uint8_t *data;                  /* Pixel data in PSRAM (NULL if not loaded) */
    lv_image_dsc_t lvgl_dsc;        /* LVGL image descriptor */
} ss_loaded_image_t;

/* =============================================================================
 * UPLOAD STATE MACHINE
 * ========================================================================== */

typedef enum {
    IMG_UPLOAD_IDLE = 0,
    IMG_UPLOAD_RECEIVING,
    IMG_UPLOAD_COMPLETE,
    IMG_UPLOAD_ERROR
} img_upload_state_t;

typedef struct {
    img_upload_state_t state;
    ss_image_slot_t slot;           /* Target slot (0-3) */
    uint32_t expected_size;         /* Total bytes expected */
    uint32_t received_size;         /* Bytes received so far */
    uint8_t *buffer;                /* Receive buffer in PSRAM */
    uint32_t crc32;                 /* Running CRC32 */
} img_upload_ctx_t;

/* =============================================================================
 * FUNCTION PROTOTYPES
 * ========================================================================== */

/**
 * @brief Initialize screensaver image system
 * Allocates PSRAM for image loading, checks for existing images on LFS
 */
void ss_images_init(void);

/**
 * @brief Load screensaver image from LittleFS
 * @param slot Image slot (SS_IMG_CPU, SS_IMG_GPU, etc.)
 * @return true if loaded successfully, false to use compiled fallback
 */
bool ss_image_load(ss_image_slot_t slot);

/**
 * @brief Get LVGL image descriptor for a slot
 * @param slot Image slot
 * @return Pointer to lv_image_dsc_t (either loaded or compiled fallback)
 */
const lv_image_dsc_t *ss_image_get_dsc(ss_image_slot_t slot);

/**
 * @brief Check if custom image is loaded for slot
 * @param slot Image slot
 * @return true if custom image loaded from LFS
 */
bool ss_image_is_custom(ss_image_slot_t slot);

/**
 * @brief Free loaded image from PSRAM
 * @param slot Image slot
 */
void ss_image_unload(ss_image_slot_t slot);

/**
 * @brief Save image data to LittleFS
 * @param slot Image slot
 * @param data Raw image data (header + pixels)
 * @param size Total size
 * @return true if saved successfully
 */
bool ss_image_save(ss_image_slot_t slot, const uint8_t *data, uint32_t size);

/**
 * @brief Delete custom image from LittleFS
 * @param slot Image slot
 * @return true if deleted (or didn't exist)
 */
bool ss_image_delete(ss_image_slot_t slot);

/* =============================================================================
 * UPLOAD PROTOCOL HANDLERS
 * ========================================================================== */

/**
 * @brief Handle IMG_BEGIN command - Start image upload
 * Format: IMG_BEGIN:slot:size (e.g., "IMG_BEGIN:0:172816")
 * @param line Command line
 * @return true if command handled
 */
bool handle_img_begin(const char *line);

/**
 * @brief Handle IMG_DATA command - Receive hex-encoded chunk
 * Format: IMG_DATA:offset:hexdata (e.g., "IMG_DATA:0:53434152...")
 * @param line Command line
 * @return true if command handled
 */
bool handle_img_data(const char *line);

/**
 * @brief Handle IMG_END command - Finalize upload
 * Format: IMG_END:crc32 (e.g., "IMG_END:A1B2C3D4")
 * @param line Command line
 * @return true if command handled
 */
bool handle_img_end(const char *line);

/**
 * @brief Handle IMG_ABORT command - Cancel upload
 * @return true if command handled
 */
bool handle_img_abort(void);

/**
 * @brief Handle IMG_DELETE command - Delete custom image
 * Format: IMG_DELETE:slot (e.g., "IMG_DELETE:0")
 * @param line Command line
 * @return true if command handled
 */
bool handle_img_delete(const char *line);

/**
 * @brief Handle IMG_STATUS command - Query upload/image status
 * @return true if command handled
 */
bool handle_img_status(void);

/**
 * @brief Main image command dispatcher
 * @param line Command line starting with "IMG_"
 * @return true if any IMG_ command was handled
 */
bool handle_image_command(const char *line);

#endif /* SCREENSAVER_IMAGES_H */
