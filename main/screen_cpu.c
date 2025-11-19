#include "screens.h"
#include "graphics.h"
#include <stdio.h>
#include <math.h>

#define CPU_RING_RADIUS 90
#define CPU_RING_THICKNESS 18
#define CPU_CENTER_X 120
#define CPU_CENTER_Y 120

void screen_cpu_init(gc9a01_handle_t *display) {
    // Clear screen to black
    gc9a01_fill_screen(display, COLOR_BLACK);
    
    // Draw "CPU" label at top
    graphics_draw_string_centered(display, 30, "CPU", COLOR_GRAY, FONT_MEDIUM);
}

void screen_cpu_update(gc9a01_handle_t *display, const pc_stats_t *stats) {
    static uint8_t last_cpu_percent = 255; // Invalid value to force first update
    static float last_cpu_temp = -1.0f;
    
    // Only redraw if values changed significantly
    if (stats->cpu_percent == last_cpu_percent && 
        fabsf(stats->cpu_temp - last_cpu_temp) < 0.5f) {
        return; // No significant change
    }
    
    last_cpu_percent = stats->cpu_percent;
    last_cpu_temp = stats->cpu_temp;
    
    // Clear center area (smaller rectangle, not full circle)
    gc9a01_fill_rect(display, 60, 60, 120, 120, COLOR_BLACK);
    
    // Draw background ring (gray) - simplified
    // Skip background ring for speed - just draw active part
    
    // Draw CPU percentage ring (blue to purple gradient)
    uint16_t color_start = RGB565(102, 126, 234); // #667eea
    uint16_t color_end = RGB565(118, 75, 162);    // #764ba2
    graphics_draw_ring_gauge(display, CPU_CENTER_X, CPU_CENTER_Y, CPU_RING_RADIUS,
                            CPU_RING_THICKNESS, stats->cpu_percent, color_start, color_end);
    
    // Draw percentage text
    char percent_str[8];
    snprintf(percent_str, sizeof(percent_str), "%d%%", stats->cpu_percent);
    
    uint16_t text_y = CPU_CENTER_Y - 20;
    graphics_draw_string_centered(display, text_y, percent_str, COLOR_WHITE, FONT_XLARGE);
    
    // Draw temperature
    char temp_str[16];
    snprintf(temp_str, sizeof(temp_str), "%.1fC", stats->cpu_temp);
    
    uint16_t temp_color = COLOR_GREEN;
    if (stats->cpu_temp > 70) temp_color = RGB565(255, 68, 68);
    else if (stats->cpu_temp > 60) temp_color = RGB565(255, 107, 107);
    
    graphics_draw_string_centered(display, CPU_CENTER_Y + 25, temp_str, temp_color, FONT_MEDIUM);
}
