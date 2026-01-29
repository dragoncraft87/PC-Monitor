/**
 * @file storage_mgr.c
 * @brief LittleFS Storage Manager Implementation
 */

#include "storage_mgr.h"
#include "esp_littlefs.h"
#include "esp_log.h"

static const char *TAG = "STORAGE";

static bool s_is_mounted = false;
static size_t s_total_bytes = 0;
static size_t s_used_bytes = 0;

esp_err_t storage_init(void)
{
    ESP_LOGI(TAG, "Initializing LittleFS storage...");

    esp_vfs_littlefs_conf_t conf = {
        .base_path = STORAGE_MOUNT_POINT,
        .partition_label = "storage",
        .format_if_mount_failed = true,  /* Desert-Spec: auto-format on error */
        .dont_mount = false,
    };

    esp_err_t ret = esp_vfs_littlefs_register(&conf);

    if (ret == ESP_OK) {
        esp_littlefs_info(conf.partition_label, &s_total_bytes, &s_used_bytes);
        ESP_LOGI(TAG, "LittleFS mounted: %u KB total, %u KB used",
                 (unsigned)(s_total_bytes / 1024), (unsigned)(s_used_bytes / 1024));
        s_is_mounted = true;
    } else if (ret == ESP_ERR_NOT_FOUND) {
        ESP_LOGE(TAG, "LittleFS partition 'storage' not found!");
    } else {
        ESP_LOGE(TAG, "LittleFS mount failed: %s", esp_err_to_name(ret));
    }

    return ret;
}

bool storage_is_mounted(void)
{
    return s_is_mounted;
}

void storage_get_info(size_t *total_kb, size_t *used_kb)
{
    if (total_kb) *total_kb = s_total_bytes / 1024;
    if (used_kb) *used_kb = s_used_bytes / 1024;
}
