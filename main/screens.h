#ifndef SCREENS_H
#define SCREENS_H

#include "gc9a01.h"
#include <stdint.h>

// PC Stats data structure
typedef struct {
    // CPU
    uint8_t cpu_percent;
    float cpu_temp;
    
    // GPU
    uint8_t gpu_percent;
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
    uint8_t net_history[60]; // Traffic history for graph
} pc_stats_t;

// Screen 1: CPU Gauge
void screen_cpu_init(gc9a01_handle_t *display);
void screen_cpu_update(gc9a01_handle_t *display, const pc_stats_t *stats);

// Screen 2: GPU Gauge
void screen_gpu_init(gc9a01_handle_t *display);
void screen_gpu_update(gc9a01_handle_t *display, const pc_stats_t *stats);

// Screen 3: RAM Bar
void screen_ram_init(gc9a01_handle_t *display);
void screen_ram_update(gc9a01_handle_t *display, const pc_stats_t *stats);

// Screen 4: Cyberpunk Network
void screen_network_init(gc9a01_handle_t *display);
void screen_network_update(gc9a01_handle_t *display, const pc_stats_t *stats);

#endif // SCREENS_H
