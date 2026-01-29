/**
 * @file storage_mgr.h
 * @brief LittleFS Storage Manager
 *
 * Handles LittleFS initialization with auto-format on error.
 */

#ifndef STORAGE_MGR_H
#define STORAGE_MGR_H

#include "esp_err.h"
#include <stdbool.h>

/* Mount point for LittleFS */
#define STORAGE_MOUNT_POINT "/storage"

/**
 * @brief Initialize LittleFS storage
 *
 * Mounts LittleFS partition with auto-format on failure (Desert-Spec requirement).
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t storage_init(void);

/**
 * @brief Check if storage is mounted
 * @return true if mounted successfully
 */
bool storage_is_mounted(void);

/**
 * @brief Get storage usage info
 * @param total_kb Pointer to store total size in KB (can be NULL)
 * @param used_kb Pointer to store used size in KB (can be NULL)
 */
void storage_get_info(size_t *total_kb, size_t *used_kb);

#endif /* STORAGE_MGR_H */
