/**
 * @file system_types.h
 * @brief Centralized system data types
 *
 * This header contains shared data structures used across multiple modules.
 * Include this header instead of defining types locally.
 */

#ifndef SYSTEM_TYPES_H
#define SYSTEM_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief PC Stats data structure
 *
 * Contains hardware monitoring data received from the PC client.
 * A value of -1 indicates sensor error / N/A.
 */
typedef struct {
    /* CPU */
    int16_t cpu_percent;        /**< CPU usage 0-100, -1 = error */
    float cpu_temp;             /**< CPU temperature in Celsius, -1 = error */

    /* GPU */
    int16_t gpu_percent;        /**< GPU usage 0-100, -1 = error */
    float gpu_temp;             /**< GPU temperature in Celsius, -1 = error */
    float gpu_vram_used;        /**< VRAM used in GB */
    float gpu_vram_total;       /**< VRAM total in GB */

    /* RAM */
    float ram_used_gb;          /**< RAM used in GB, -1 = error */
    float ram_total_gb;         /**< RAM total in GB, -1 = error */

    /* Network */
    char net_type[16];          /**< Connection type: "LAN" or "WLAN" */
    char net_speed[16];         /**< Link speed: "1000 Mbps" etc */
    float net_down_mbps;        /**< Download speed in Mbps, -1 = error */
    float net_up_mbps;          /**< Upload speed in Mbps, -1 = error */
} pc_stats_t;

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_TYPES_H */
