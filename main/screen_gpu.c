#include "screens.h"
#include "graphics.h"
#include <stdio.h>
#include <math.h>

#define GPU_RING_RADIUS 90
#define GPU_RING_THICKNESS 18
#define GPU_CENTER_X 120
#define GPU_CENTER_Y 120

void screen_gpu_init(gc9a01_handle_t *display) {
    // Clear screen to black
    gc9a01_fill_screen(display, COLOR_BLACK);
    
    // Draw "GPU" label at top
    graphics_draw_string_centered(display, 30, "GPU", COLOR_GRAY, FONT_MEDIUM);
}

void screen_gpu_update(gc9a01_handle_t *display, const pc_stats_t *stats) {
    static uint8_t last_gpu_percent = 255;
    static float last_gpu_temp = -1.0f;
    
    // Only update if changed
    if (stats->gpu_percent == last_gpu_percent && 
        fabsf(stats->gpu_temp - last_gpu_temp) < 0.5f) {
        return;
    }
    
    last_gpu_percent = stats->gpu_percent;
    last_gpu_temp = stats->gpu_temp;
    
    // Clear center area
    gc9a01_fill_rect(display, 60, 60, 120, 120, COLOR_BLACK);
    
    // Draw GPU percentage ring (cyan to blue gradient)
    uint16_t color_start = RGB565(76, 201, 240);  // #4cc9f0
    uint16_t color_end = RGB565(67, 97, 238);     // #4361ee
    graphics_draw_ring_gauge(display, GPU_CENTER_X, GPU_CENTER_Y, GPU_RING_RADIUS,
                            GPU_RING_THICKNESS, stats->gpu_percent, color_start, color_end);
    
    // Draw percentage text
    char percent_str[8];
    snprintf(percent_str, sizeof(percent_str), "%d%%", stats->gpu_percent);
    
    uint16_t text_y = GPU_CENTER_Y - 25;
    graphics_draw_string_centered(display, text_y, percent_str, COLOR_WHITE, FONT_XLARGE);
    
    // Draw temperature
    char temp_str[16];
    snprintf(temp_str, sizeof(temp_str), "%.1fC", stats->gpu_temp);
    
    uint16_t temp_color = COLOR_GREEN;
    if (stats->gpu_temp > 75) temp_color = RGB565(255, 68, 68);
    else if (stats->gpu_temp > 65) temp_color = RGB565(255, 107, 107);
    
    graphics_draw_string_centered(display, GPU_CENTER_Y + 10, temp_str, temp_color, FONT_SMALL);
    
    // Draw VRAM usage
    char vram_str[24];
    snprintf(vram_str, sizeof(vram_str), "%.1f/%.0fGB", stats->gpu_vram_used, stats->gpu_vram_total);
    graphics_draw_string_centered(display, GPU_CENTER_Y + 30, vram_str, COLOR_GREEN, FONT_SMALL);
}
