#include "screens.h"
#include "graphics.h"
#include <stdio.h>

#define RAM_BAR_WIDTH 180
#define RAM_BAR_HEIGHT 25
#define RAM_BAR_X ((240 - RAM_BAR_WIDTH) / 2)
#define RAM_BAR_Y 130

void screen_ram_init(gc9a01_handle_t *display) {
    // Clear screen to black
    gc9a01_fill_screen(display, COLOR_BLACK);
    
    // Draw "RAM" label at top
    graphics_draw_string_centered(display, 40, "RAM", COLOR_GRAY, FONT_MEDIUM);
}

void screen_ram_update(gc9a01_handle_t *display, const pc_stats_t *stats) {
    // Calculate percentage
    uint8_t ram_percent = (uint8_t)((stats->ram_used_gb / stats->ram_total_gb) * 100);
    
    // Clear center area
    gc9a01_fill_rect(display, 30, 70, 180, 100, COLOR_BLACK);
    
    // Draw RAM value (large)
    char value_str[16];
    snprintf(value_str, sizeof(value_str), "%.1f GB", stats->ram_used_gb);
    graphics_draw_string_centered(display, 80, value_str, COLOR_WHITE, FONT_LARGE);
    
    // Draw percentage
    char percent_str[8];
    snprintf(percent_str, sizeof(percent_str), "%d%%", ram_percent);
    graphics_draw_string_centered(display, 110, percent_str, RGB565(67, 233, 123), FONT_MEDIUM);
    
    // Draw progress bar background
    gc9a01_fill_rect(display, RAM_BAR_X, RAM_BAR_Y, RAM_BAR_WIDTH, RAM_BAR_HEIGHT, RGB565(34, 34, 34));
    
    // Determine bar color based on usage
    uint16_t bar_color;
    if (ram_percent > 85) {
        bar_color = RGB565(255, 68, 68);  // Red
    } else if (ram_percent > 70) {
        bar_color = RGB565(255, 165, 0);  // Orange
    } else {
        bar_color = RGB565(67, 233, 123); // Green
    }
    
    // Draw filled portion
    uint16_t filled_width = (RAM_BAR_WIDTH * ram_percent) / 100;
    graphics_draw_progress_bar(display, RAM_BAR_X, RAM_BAR_Y, RAM_BAR_WIDTH, 
                               RAM_BAR_HEIGHT, ram_percent, bar_color);
    
    // Draw segments (8 vertical lines)
    for (int i = 1; i < 8; i++) {
        uint16_t x = RAM_BAR_X + (RAM_BAR_WIDTH * i / 8);
        gc9a01_draw_line(display, x, RAM_BAR_Y, x, RAM_BAR_Y + RAM_BAR_HEIGHT, COLOR_BLACK);
    }
    
    // Draw "of XX GB" text
    char total_str[16];
    snprintf(total_str, sizeof(total_str), "of %.0f GB", stats->ram_total_gb);
    graphics_draw_string_centered(display, RAM_BAR_Y + RAM_BAR_HEIGHT + 15, total_str, COLOR_GRAY, FONT_SMALL);
}
