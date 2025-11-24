/**
 * @file screen_network_lvgl.c
 * @brief Network Graph Screen (Display 4) - LVGL Implementation
 *
 * Design based on cyberpunk-style.html:
 * - Cyberpunk aesthetic (cyan/magenta colors)
 * - Connection type (LAN/WiFi) and speed
 * - Real-time traffic graph (Chart widget)
 * - Upload/Download speeds
 */

#include "screens_lvgl.h"
#include <stdio.h>

#define NETWORK_HISTORY_SIZE 60

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

    /* Traffic history */
    int history_index;
};

screen_network_t *screen_network_create(lv_display_t *disp)
{
    screen_network_t *s = malloc(sizeof(screen_network_t));
    if (!s) return NULL;

    s->history_index = 0;

    /* Set this display as default temporarily */
    lv_display_t *old_default = lv_display_get_default();
    lv_display_set_default(disp);

    /* Create screen */
    s->screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s->screen, lv_color_black(), 0);

    /* ========================================================================
     * HEADER LABELS
     * ====================================================================== */

    /* "NETWORK.SYS" Header */
    s->label_header = lv_label_create(s->screen);
    lv_label_set_text(s->label_header, "NETWORK");
    lv_obj_set_style_text_font(s->label_header, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s->label_header, lv_color_make(0x00, 0xff, 0xff), 0);
    lv_obj_align(s->label_header, LV_ALIGN_TOP_MID, 0, 25);

    /* Connection Type (LAN/WiFi) */
    s->label_conn_type = lv_label_create(s->screen);
    lv_label_set_text(s->label_conn_type, "LAN");
    lv_obj_set_style_text_font(s->label_conn_type, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s->label_conn_type, lv_color_make(0x00, 0xff, 0xff), 0);
    lv_obj_align(s->label_conn_type, LV_ALIGN_TOP_MID, 0, 45);

    /* Speed Indicator */
    s->label_speed = lv_label_create(s->screen);
    lv_label_set_text(s->label_speed, "1000 Mbps");
    lv_obj_set_style_text_font(s->label_speed, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s->label_speed, lv_color_make(0xff, 0x00, 0xff), 0);
    lv_obj_align(s->label_speed, LV_ALIGN_TOP_MID, 0, 68);

    /* ========================================================================
     * CHART WIDGET (Traffic Graph)
     * ====================================================================== */
    s->chart = lv_chart_create(s->screen);
    lv_obj_set_size(s->chart, 180, 60);
    lv_obj_align(s->chart, LV_ALIGN_CENTER, 0, 10);

    lv_chart_set_type(s->chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(s->chart, NETWORK_HISTORY_SIZE);
    lv_chart_set_range(s->chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_set_update_mode(s->chart, LV_CHART_UPDATE_MODE_SHIFT);

    /* Styling */
    lv_obj_set_style_bg_color(s->chart, lv_color_make(0x00, 0x14, 0x28), 0);
    lv_obj_set_style_border_color(s->chart, lv_color_make(0x00, 0xff, 0xff), 0);
    lv_obj_set_style_border_width(s->chart, 1, 0);

    /* Create series for download (cyan) and upload (magenta) */
    s->ser_down = lv_chart_add_series(s->chart, lv_color_make(0x00, 0xff, 0xff), LV_CHART_AXIS_PRIMARY_Y);
    s->ser_up = lv_chart_add_series(s->chart, lv_color_make(0xff, 0x00, 0xff), LV_CHART_AXIS_PRIMARY_Y);

    /* Initialize with zeros */
    for (int i = 0; i < NETWORK_HISTORY_SIZE; i++) {
        lv_chart_set_next_value(s->chart, s->ser_down, 0);
        lv_chart_set_next_value(s->chart, s->ser_up, 0);
    }

    /* ========================================================================
     * TRAFFIC STATS (Download/Upload)
     * ====================================================================== */

    /* Download Speed */
    s->label_down = lv_label_create(s->screen);
    lv_label_set_text(s->label_down, "DN: 0 MB/s");
    lv_obj_set_style_text_font(s->label_down, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s->label_down, lv_color_make(0x00, 0xff, 0xff), 0);
    lv_obj_align(s->label_down, LV_ALIGN_BOTTOM_LEFT, 20, -20);

    /* Upload Speed */
    s->label_up = lv_label_create(s->screen);
    lv_label_set_text(s->label_up, "UP: 0 MB/s");
    lv_obj_set_style_text_font(s->label_up, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s->label_up, lv_color_make(0xff, 0x00, 0xff), 0);
    lv_obj_align(s->label_up, LV_ALIGN_BOTTOM_RIGHT, -20, -20);

    /* Load screen to this display */
    lv_screen_load(s->screen);

    /* Restore previous default display */
    if (old_default) {
        lv_display_set_default(old_default);
    }

    return s;
}

void screen_network_update(screen_network_t *s, const pc_stats_t *stats)
{
    if (!s) return;

    /* Update connection type */
    lv_label_set_text(s->label_conn_type, stats->net_type);

    /* Update speed */
    lv_label_set_text(s->label_speed, stats->net_speed);

    /* Update download speed label */
    char down_buf[16];
    snprintf(down_buf, sizeof(down_buf), "DN: %.1f MB/s", stats->net_down_mbps);
    lv_label_set_text(s->label_down, down_buf);

    /* Update upload speed label */
    char up_buf[16];
    snprintf(up_buf, sizeof(up_buf), "UP: %.1f MB/s", stats->net_up_mbps);
    lv_label_set_text(s->label_up, up_buf);

    /* Add new data point to chart (scale to 0-100 range) */
    /* Assuming max 20 MB/s for scaling */
    int down_scaled = (int)((stats->net_down_mbps / 20.0f) * 100.0f);
    int up_scaled = (int)((stats->net_up_mbps / 20.0f) * 100.0f);

    if (down_scaled > 100) down_scaled = 100;
    if (up_scaled > 100) up_scaled = 100;

    lv_chart_set_next_value(s->chart, s->ser_down, down_scaled);
    lv_chart_set_next_value(s->chart, s->ser_up, up_scaled);

    lv_chart_refresh(s->chart);
}
