/**
 * @file screen_cpu_lvgl.c
 * @brief CPU Gauge Screen (Display 1) - LVGL Implementation
 *
 * Design based on cpu-gauge.html:
 * - Ring gauge (Arc widget) showing CPU percentage
 * - Gradient: Blue (#667eea) → Purple (#764ba2)
 * - Center text: "CPU", percentage value, temperature
 * - Temperature changes color based on value
 */

#include "screens_lvgl.h"
#include <stdio.h>

/* Widget handles */
struct screen_cpu_t {
    lv_obj_t *screen;
    lv_obj_t *arc;
    lv_obj_t *label_title;
    lv_obj_t *label_percent;
    lv_obj_t *label_temp;
};

/**
 * @brief Create CPU gauge screen
 */
screen_cpu_t *screen_cpu_create(lv_display_t *disp)
{
    screen_cpu_t *s = malloc(sizeof(screen_cpu_t));
    if (!s) return NULL;

    /* Set this display as default temporarily to create screen on it */
    lv_display_t *old_default = lv_display_get_default();
    lv_display_set_default(disp);

    /* Create screen (will be created on the default display) */
    s->screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s->screen, lv_color_black(), 0);

    /* ========================================================================
     * ARC WIDGET (Ring Gauge)
     * ====================================================================== */
    s->arc = lv_arc_create(s->screen);
    lv_obj_set_size(s->arc, 200, 200);
    lv_obj_center(s->arc);

    /* Arc configuration */
    lv_arc_set_range(s->arc, 0, 100);
    lv_arc_set_value(s->arc, 0);
    lv_arc_set_bg_angles(s->arc, 135, 45);  /* Bottom-left to top-right */
    lv_arc_set_rotation(s->arc, 0);

    /* Styling */
    lv_obj_set_style_arc_width(s->arc, 20, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s->arc, 20, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s->arc, lv_color_make(0x22, 0x22, 0x22), LV_PART_MAIN);

    /* Gradient: Blue → Purple (approximation, LVGL doesn't support arc gradients easily) */
    lv_obj_set_style_arc_color(s->arc, lv_color_make(0x66, 0x7e, 0xea), LV_PART_INDICATOR);

    /* Remove knob (not needed for gauge) */
    lv_obj_set_style_bg_opa(s->arc, 0, LV_PART_KNOB);

    /* ========================================================================
     * CENTER LABELS
     * ====================================================================== */

    /* "CPU" Title */
    s->label_title = lv_label_create(s->screen);
    lv_label_set_text(s->label_title, "CPU");
    lv_obj_set_style_text_font(s->label_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s->label_title, lv_color_make(0x88, 0x88, 0x88), 0);
    lv_obj_align(s->label_title, LV_ALIGN_CENTER, 0, -35);

    /* Percentage Value */
    s->label_percent = lv_label_create(s->screen);
    lv_label_set_text(s->label_percent, "0%");
    lv_obj_set_style_text_font(s->label_percent, &lv_font_montserrat_42, 0);
    lv_obj_set_style_text_color(s->label_percent, lv_color_white(), 0);
    lv_obj_align(s->label_percent, LV_ALIGN_CENTER, 0, -5);

    /* Temperature */
    s->label_temp = lv_label_create(s->screen);
    lv_label_set_text(s->label_temp, "0.0C");
    lv_obj_set_style_text_font(s->label_temp, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s->label_temp, lv_color_make(0x4c, 0xaf, 0x50), 0);
    lv_obj_align(s->label_temp, LV_ALIGN_CENTER, 0, 30);

    /* Load screen to this display */
    lv_screen_load(s->screen);

    /* Restore previous default display */
    if (old_default) {
        lv_display_set_default(old_default);
    }

    return s;
}

/**
 * @brief Update CPU screen with new data
 */
void screen_cpu_update(screen_cpu_t *s, const pc_stats_t *stats)
{
    if (!s) return;

    /* Update arc value */
    lv_arc_set_value(s->arc, stats->cpu_percent);

    /* Update percentage label */
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", stats->cpu_percent);
    lv_label_set_text(s->label_percent, buf);

    /* Update temperature label */
    char temp_buf[16];
    snprintf(temp_buf, sizeof(temp_buf), "%.1fC", stats->cpu_temp);
    lv_label_set_text(s->label_temp, temp_buf);

    /* Change temperature color based on value */
    lv_color_t temp_color;
    if (stats->cpu_temp > 70.0f) {
        temp_color = lv_color_make(0xff, 0x44, 0x44); /* Red */
    } else if (stats->cpu_temp > 60.0f) {
        temp_color = lv_color_make(0xff, 0x6b, 0x6b); /* Orange-Red */
    } else {
        temp_color = lv_color_make(0x4c, 0xaf, 0x50); /* Green */
    }
    lv_obj_set_style_text_color(s->label_temp, temp_color, 0);
}
