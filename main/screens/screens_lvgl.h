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
#include "core/system_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Screen structures (exposed for theme access)
 */
#define NETWORK_HISTORY_SIZE 60

struct screen_cpu_t {
    lv_obj_t *screen;
    lv_obj_t *arc;
    lv_obj_t *label_title;
    lv_obj_t *label_percent;
    lv_obj_t *label_temp;
};

struct screen_gpu_t {
    lv_obj_t *screen;
    lv_obj_t *arc;
    lv_obj_t *label_title;
    lv_obj_t *label_percent;
    lv_obj_t *label_temp;
    lv_obj_t *label_vram;
};

struct screen_ram_t {
    lv_obj_t *screen;
    lv_obj_t *label_title;
    lv_obj_t *label_value;
    lv_obj_t *label_percent;
    lv_obj_t *bar;
    lv_obj_t *label_total;
};

struct screen_network_t {
    lv_obj_t *screen;
    lv_obj_t *label_header;
    lv_obj_t *label_conn_type;
    lv_obj_t *label_speed;
    lv_obj_t *chart;
    lv_chart_series_t *ser_down;
    lv_chart_series_t *ser_up;
    lv_obj_t *label_down;
    lv_obj_t *label_up;
    int history_index;
};

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
