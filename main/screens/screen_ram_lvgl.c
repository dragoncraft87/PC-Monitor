/**
 * @file screen_ram_lvgl.c
 * @brief RAM Bar Screen (Display 3) - LVGL Implementation
 *
 * Design based on ram-bars.html:
 * - Horizontal progress bar (Bar widget)
 * - Gradient: Green (#43e97b) → Turquoise (#38f9d7)
 * - Segments for visual clarity
 * - Shows RAM used, percentage, total
 */

#include "screens_lvgl.h"
#include <stdio.h>

struct screen_ram_t {
    lv_obj_t *screen;
    lv_obj_t *label_title;
    lv_obj_t *label_value;
    lv_obj_t *label_percent;
    lv_obj_t *bar;
    lv_obj_t *label_total;
};

screen_ram_t *screen_ram_create(lv_display_t *disp)
{
    screen_ram_t *s = malloc(sizeof(screen_ram_t));
    if (!s) return NULL;

    /* Set this display as default temporarily */
    lv_display_t *old_default = lv_display_get_default();
    lv_display_set_default(disp);

    /* Create screen */
    s->screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s->screen, lv_color_black(), 0);

    /* ========================================================================
     * LABELS
     * ====================================================================== */

    /* "RAM" Title */
    s->label_title = lv_label_create(s->screen);
    lv_label_set_text(s->label_title, "RAM");
    lv_obj_set_style_text_font(s->label_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s->label_title, lv_color_make(0x88, 0x88, 0x88), 0);
    lv_obj_align(s->label_title, LV_ALIGN_TOP_MID, 0, 40);

    /* Used RAM Value */
    s->label_value = lv_label_create(s->screen);
    lv_label_set_text(s->label_value, "0.0 GB");
    lv_obj_set_style_text_font(s->label_value, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(s->label_value, lv_color_white(), 0);
    lv_obj_align(s->label_value, LV_ALIGN_CENTER, 0, -30);

    /* Percentage */
    s->label_percent = lv_label_create(s->screen);
    lv_label_set_text(s->label_percent, "0%");
    lv_obj_set_style_text_font(s->label_percent, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(s->label_percent, lv_color_make(0x43, 0xe9, 0x7b), 0);
    lv_obj_align(s->label_percent, LV_ALIGN_CENTER, 0, 0);

    /* ========================================================================
     * PROGRESS BAR
     * ====================================================================== */
    s->bar = lv_bar_create(s->screen);
    lv_obj_set_size(s->bar, 180, 25);
    lv_obj_align(s->bar, LV_ALIGN_CENTER, 0, 30);

    lv_bar_set_range(s->bar, 0, 100);
    lv_bar_set_value(s->bar, 0, LV_ANIM_OFF);

    /* Styling */
    lv_obj_set_style_bg_color(s->bar, lv_color_make(0x22, 0x22, 0x22), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s->bar, lv_color_make(0x43, 0xe9, 0x7b), LV_PART_INDICATOR);
    lv_obj_set_style_radius(s->bar, 15, LV_PART_MAIN);
    lv_obj_set_style_radius(s->bar, 15, LV_PART_INDICATOR);

    /* Total RAM */
    s->label_total = lv_label_create(s->screen);
    lv_label_set_text(s->label_total, "von 16 GB");
    lv_obj_set_style_text_font(s->label_total, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s->label_total, lv_color_make(0x88, 0x88, 0x88), 0);
    lv_obj_align(s->label_total, LV_ALIGN_CENTER, 0, 60);

    /* Load screen to this display */
    lv_screen_load(s->screen);

    /* Restore previous default display */
    if (old_default) {
        lv_display_set_default(old_default);
    }

    return s;
}

void screen_ram_update(screen_ram_t *s, const pc_stats_t *stats)
{
    if (!s) return;

    /* Calculate percentage */
    int percent = (int)((stats->ram_used_gb / stats->ram_total_gb) * 100.0f);

    /* Update bar */
    lv_bar_set_value(s->bar, percent, LV_ANIM_ON);

    /* Update used RAM value */
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f GB", stats->ram_used_gb);
    lv_label_set_text(s->label_value, buf);

    /* Update percentage */
    char percent_buf[8];
    snprintf(percent_buf, sizeof(percent_buf), "%d%%", percent);
    lv_label_set_text(s->label_percent, percent_buf);

    /* Update total */
    char total_buf[16];
    snprintf(total_buf, sizeof(total_buf), "von %.0f GB", stats->ram_total_gb);
    lv_label_set_text(s->label_total, total_buf);

    /* Change bar color based on usage */
    lv_color_t bar_color;
    if (percent > 85) {
        bar_color = lv_color_make(0xff, 0x44, 0x44); /* Red */
    } else if (percent > 70) {
        bar_color = lv_color_make(0xff, 0xa5, 0x00); /* Orange */
    } else {
        bar_color = lv_color_make(0x43, 0xe9, 0x7b); /* Green */
    }
    lv_obj_set_style_bg_color(s->bar, bar_color, LV_PART_INDICATOR);
}
