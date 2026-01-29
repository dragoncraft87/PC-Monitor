/**
 * @file screensaver_mgr.h
 * @brief Screensaver Image Management - LittleFS Storage
 *
 * Supports RGB565A8 format (16-bit color + 8-bit alpha) for transparency
 */

#ifndef SCREENSAVER_MGR_H
#define SCREENSAVER_MGR_H

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
#define SCARAB_IMG_PIXELS       (SCARAB_IMG_WIDTH * SCARAB_IMG_HEIGHT)

#define SCARAB_RGB565_SIZE      (SCARAB_IMG_PIXELS * 2)     /* 115200 bytes */
#define SCARAB_RGB565A8_SIZE    (SCARAB_IMG_PIXELS * 3)     /* 172800 bytes */

#define SCARAB_IMG_HEADER_SIZE  sizeof(scarab_img_header_t)
#define SCARAB_IMG_MAX_SIZE     (SCARAB_IMG_HEADER_SIZE + SCARAB_RGB565A8_SIZE)

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
 * FUNCTION PROTOTYPES
 * ========================================================================== */

/**
 * @brief Initialize screensaver image system
 */
void ss_images_init(void);

/**
 * @brief Load screensaver image from LittleFS
 * @param slot Image slot
 * @return true if loaded successfully
 */
bool ss_image_load(ss_image_slot_t slot);

/**
 * @brief Get LVGL image descriptor for a slot
 * @param slot Image slot
 * @return Pointer to lv_image_dsc_t
 */
const lv_image_dsc_t *ss_image_get_dsc(ss_image_slot_t slot);

/**
 * @brief Check if custom image is loaded for slot
 */
bool ss_image_is_custom(ss_image_slot_t slot);

/**
 * @brief Free loaded image from PSRAM
 */
void ss_image_unload(ss_image_slot_t slot);

/**
 * @brief Save image data to LittleFS
 */
bool ss_image_save(ss_image_slot_t slot, const uint8_t *data, uint32_t size);

/**
 * @brief Delete custom image from LittleFS
 */
bool ss_image_delete(ss_image_slot_t slot);

/**
 * @brief Main image command dispatcher
 * @param line Command line starting with "IMG_"
 * @return true if command was handled
 */
bool ss_image_handle_command(const char *line);

#endif /* SCREENSAVER_MGR_H */
