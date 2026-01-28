/**
 * @file screens_lvgl.h
 * @brief LVGL-based screen interfaces for 4x GC9A01 displays
 *
 * Replaces the old graphics.c-based screens with LVGL widgets:
 * - Display 1: CPU Gauge (Arc widget)
 * - Display 2: GPU Gauge (Arc widget)
 * - Display 3: RAM Bar (Bar widget)
 * - Display 4: Network Graph (Chart widget)
 */

#ifndef SCREENS_LVGL_H
#define SCREENS_LVGL_H

#include "lvgl.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief PC Stats data structure
 */
typedef struct {
    // CPU (-1 = sensor error / N/A)
    int16_t cpu_percent;
    float cpu_temp;

    // GPU (-1 = sensor error / N/A)
    int16_t gpu_percent;
    float gpu_temp;
    float gpu_vram_used;
    float gpu_vram_total;

    // RAM
    float ram_used_gb;
    float ram_total_gb;

    // Network
    char net_type[16];      // "LAN" or "WiFi"
    char net_speed[16];     // "1000 Mbps"
    float net_down_mbps;
    float net_up_mbps;
} pc_stats_t;

/**
 * @brief Screen handles (opaque structures)
 */
typedef struct screen_cpu_t screen_cpu_t;
typedef struct screen_gpu_t screen_gpu_t;
typedef struct screen_ram_t screen_ram_t;
typedef struct screen_network_t screen_network_t;

/* ============================================================================
 * SCREEN 1: CPU GAUGE (Ring with percentage and temperature)
 * ========================================================================== */
screen_cpu_t *screen_cpu_create(lv_display_t *disp);
void screen_cpu_update(screen_cpu_t *screen, const pc_stats_t *stats);

/* ============================================================================
 * SCREEN 2: GPU GAUGE (Ring with percentage, temperature, VRAM)
 * ========================================================================== */
screen_gpu_t *screen_gpu_create(lv_display_t *disp);
void screen_gpu_update(screen_gpu_t *screen, const pc_stats_t *stats);

/* ============================================================================
 * SCREEN 3: RAM BAR (Horizontal bar with segments)
 * ========================================================================== */
screen_ram_t *screen_ram_create(lv_display_t *disp);
void screen_ram_update(screen_ram_t *screen, const pc_stats_t *stats);

/* ============================================================================
 * SCREEN 4: NETWORK GRAPH (Cyberpunk style with chart)
 * ========================================================================== */
screen_network_t *screen_network_create(lv_display_t *disp);
void screen_network_update(screen_network_t *screen, const pc_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* SCREENS_LVGL_H */
