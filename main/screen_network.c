#include "screens.h"
#include "graphics.h"
#include <stdio.h>
#include <string.h>

#define GRAPH_WIDTH 170
#define GRAPH_HEIGHT 55
#define GRAPH_X ((240 - GRAPH_WIDTH) / 2)
#define GRAPH_Y 100

// Static traffic history for animation
static uint8_t traffic_history[60] = {0};
static uint8_t history_index = 0;

void screen_network_init(gc9a01_handle_t *display) {
    // Clear screen to black
    gc9a01_fill_screen(display, COLOR_BLACK);
    
    // Draw "NETWORK.SYS" label at top (cyberpunk style)
    graphics_draw_string_centered(display, 25, "NETWORK.SYS", COLOR_CYBER_CYAN, FONT_SMALL);
    
    // Draw corner accents
    // Top-left
    gc9a01_draw_line(display, 20, 35, 50, 35, COLOR_CYBER_CYAN);
    gc9a01_draw_line(display, 20, 35, 20, 65, COLOR_CYBER_CYAN);
    
    // Bottom-right
    gc9a01_draw_line(display, 190, 205, 220, 205, COLOR_CYBER_CYAN);
    gc9a01_draw_line(display, 220, 175, 220, 205, COLOR_CYBER_CYAN);
}

void draw_network_graph(gc9a01_handle_t *display) {
    // Draw graph background
    gc9a01_fill_rect(display, GRAPH_X, GRAPH_Y, GRAPH_WIDTH, GRAPH_HEIGHT, RGB565(0, 20, 40));
    
    // Draw border
    gc9a01_draw_line(display, GRAPH_X, GRAPH_Y, GRAPH_X + GRAPH_WIDTH, GRAPH_Y, COLOR_CYBER_CYAN);
    gc9a01_draw_line(display, GRAPH_X, GRAPH_Y + GRAPH_HEIGHT, GRAPH_X + GRAPH_WIDTH, GRAPH_Y + GRAPH_HEIGHT, COLOR_CYBER_CYAN);
    gc9a01_draw_line(display, GRAPH_X, GRAPH_Y, GRAPH_X, GRAPH_Y + GRAPH_HEIGHT, COLOR_CYBER_CYAN);
    gc9a01_draw_line(display, GRAPH_X + GRAPH_WIDTH, GRAPH_Y, GRAPH_X + GRAPH_WIDTH, GRAPH_Y + GRAPH_HEIGHT, COLOR_CYBER_CYAN);
    
    // Draw grid lines (5 horizontal lines)
    for (int i = 1; i < 5; i++) {
        uint16_t y = GRAPH_Y + (GRAPH_HEIGHT * i / 5);
        for (int x = GRAPH_X; x < GRAPH_X + GRAPH_WIDTH; x += 4) {
            gc9a01_draw_pixel(display, x, y, RGB565(0, 100, 100));
        }
    }
    
    // Draw traffic history as line graph
    for (int i = 1; i < 60; i++) {
        uint16_t x1 = GRAPH_X + ((GRAPH_WIDTH * (i - 1)) / 59);
        uint16_t x2 = GRAPH_X + ((GRAPH_WIDTH * i) / 59);
        
        // Scale traffic to fit graph (0-100% maps to graph height)
        uint16_t y1 = GRAPH_Y + GRAPH_HEIGHT - ((traffic_history[i - 1] * GRAPH_HEIGHT) / 100);
        uint16_t y2 = GRAPH_Y + GRAPH_HEIGHT - ((traffic_history[i] * GRAPH_HEIGHT) / 100);
        
        gc9a01_draw_line(display, x1, y1, x2, y2, COLOR_CYBER_CYAN);
    }
}

void screen_network_update(gc9a01_handle_t *display, const pc_stats_t *stats) {
    // Clear dynamic areas
    gc9a01_fill_rect(display, 50, 45, 140, 50, COLOR_BLACK);
    
    // Draw connection type (LAN/WiFi)
    graphics_draw_string_centered(display, 50, stats->net_type, COLOR_CYBER_CYAN, FONT_LARGE);
    
    // Draw speed indicator
    graphics_draw_string_centered(display, 75, stats->net_speed, COLOR_CYBER_MAGENTA, FONT_MEDIUM);
    
    // Update traffic history (add current download percentage)
    // Normalize download speed to 0-100 range (assume max 100 MB/s)
    uint8_t normalized = (uint8_t)((stats->net_down_mbps / 100.0f) * 100);
    if (normalized > 100) normalized = 100;
    
    traffic_history[history_index] = normalized;
    history_index = (history_index + 1) % 60;
    
    // Draw traffic graph
    draw_network_graph(display);
    
    // Clear stats area
    gc9a01_fill_rect(display, 30, 165, 180, 40, COLOR_BLACK);
    
    // Draw download speed (left)
    char down_str[16];
    snprintf(down_str, sizeof(down_str), "%.1f MB/s", stats->net_down_mbps);
    graphics_draw_string(display, 40, 170, "DOWN", COLOR_CYBER_CYAN, FONT_SMALL);
    graphics_draw_string(display, 40, 185, down_str, COLOR_CYBER_MAGENTA, FONT_SMALL);
    
    // Draw upload speed (right)
    char up_str[16];
    snprintf(up_str, sizeof(up_str), "%.1f MB/s", stats->net_up_mbps);
    graphics_draw_string(display, 145, 170, "UP", COLOR_CYBER_CYAN, FONT_SMALL);
    graphics_draw_string(display, 145, 185, up_str, COLOR_CYBER_MAGENTA, FONT_SMALL);
}
