/**
 * @file screen_cpu_lvgl.c
 * @brief CPU Gauge Screen (Display 1) - LVGL Implementation
 *
 * Design based on SquareLine Studio (SquareLine/screens/ui_Screen1.c):
 * - Ring gauge (Arc widget) showing CPU percentage (0-120 range)
 * - Green arc (#40FF64) on dark gray background (#55555C)
 * - Center text: "CPU" (top), percentage value (center), temperature (bottom)
 * - Temperature changes color based on value
 * - No rounded arc ends (sharp edges)
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
     * ARC WIDGET (Ring Gauge) - SquareLine Studio Design
     * ====================================================================== */
    s->arc = lv_arc_create(s->screen);
    lv_obj_set_size(s->arc, 200, 200);
    lv_obj_center(s->arc);

    /* Remove all interactive flags */
    lv_obj_remove_flag(s->arc, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK |
                       LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE |
                       LV_OBJ_FLAG_SNAPPABLE | LV_OBJ_FLAG_SCROLLABLE |
                       LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM |
                       LV_OBJ_FLAG_SCROLL_CHAIN);

    /* Arc configuration - 0-120 range for CPU percentage */
    lv_arc_set_range(s->arc, 0, 120);
    lv_arc_set_value(s->arc, 0);
    lv_arc_set_bg_angles(s->arc, 135, 45);  /* Bottom-left to top-right */
    lv_arc_set_rotation(s->arc, 0);

    /* MAIN (background arc) styling - Dark Gray */
    lv_obj_set_style_arc_color(s->arc, lv_color_hex(0x55555C), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(s->arc, 255, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s->arc, 20, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(s->arc, false, LV_PART_MAIN);  /* Sharp edges */

    /* INDICATOR (progress arc) styling - Intel Blue */
    lv_obj_set_style_arc_color(s->arc, lv_color_hex(0x0071C5), LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(s->arc, 255, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s->arc, 20, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(s->arc, false, LV_PART_INDICATOR);  /* Sharp edges */

    /* KNOB (center dot) - Hide it */
    lv_obj_set_style_opa(s->arc, 0, LV_PART_KNOB);

    /* ========================================================================
     * CENTER LABELS - SquareLine Studio Design
     * ====================================================================== */

    /* CPU Model Title - Top (Y offset -45, matching SquareLine) */
    s->label_title = lv_label_create(s->screen);
    lv_obj_set_width(s->label_title, LV_SIZE_CONTENT);
    lv_obj_set_height(s->label_title, LV_SIZE_CONTENT);
    lv_obj_set_x(s->label_title, 0);
    lv_obj_set_y(s->label_title, -45);
    lv_obj_set_align(s->label_title, LV_ALIGN_CENTER);
    lv_label_set_text(s->label_title, "i9-7980XE");
    lv_obj_set_style_text_align(s->label_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(s->label_title, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* Percentage Value - Center (Y offset 0) */
    s->label_percent = lv_label_create(s->screen);
    lv_obj_set_width(s->label_percent, LV_SIZE_CONTENT);
    lv_obj_set_height(s->label_percent, LV_SIZE_CONTENT);
    lv_obj_set_align(s->label_percent, LV_ALIGN_CENTER);
    lv_label_set_text(s->label_percent, "XX%");
    lv_obj_set_style_text_color(s->label_percent, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_opa(s->label_percent, 255, LV_PART_MAIN);
    lv_obj_set_style_text_align(s->label_percent, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(s->label_percent, &lv_font_montserrat_42, LV_PART_MAIN);

    /* Temperature - Bottom (Y offset +70) */
    s->label_temp = lv_label_create(s->screen);
    lv_obj_set_width(s->label_temp, LV_SIZE_CONTENT);
    lv_obj_set_height(s->label_temp, LV_SIZE_CONTENT);
    lv_obj_set_align(s->label_temp, LV_ALIGN_CENTER);
    lv_obj_set_pos(s->label_temp, 0, 70);
    lv_label_set_text(s->label_temp, "XX°C");
    lv_obj_set_style_text_color(s->label_temp, lv_color_hex(0xF40B0B), LV_PART_MAIN);  /* Red default */
    lv_obj_set_style_text_opa(s->label_temp, 255, LV_PART_MAIN);
    lv_obj_set_style_text_align(s->label_temp, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(s->label_temp, &lv_font_montserrat_34, LV_PART_MAIN);

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
 *
 * Note: Arc range is 0-120, so we map CPU percentage (0-100) to this range
 * This allows for visual overdrive effect if CPU > 100% (rare but possible with turbo)
 */
void screen_cpu_update(screen_cpu_t *s, const pc_stats_t *stats)
{
    if (!s) return;

    /* Update arc value - clamp to 120 max */
    int arc_value = stats->cpu_percent;
    if (arc_value > 120) arc_value = 120;
    lv_arc_set_value(s->arc, arc_value);

    /* Update percentage label */
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", stats->cpu_percent);
    lv_label_set_text(s->label_percent, buf);

    /* Update temperature label with degree symbol (no decimals) */
    char temp_buf[16];
    snprintf(temp_buf, sizeof(temp_buf), "%d°C", (int)stats->cpu_temp);
    lv_label_set_text(s->label_temp, temp_buf);

    /* Change temperature color based on value */
    lv_color_t temp_color;
    if (stats->cpu_temp > 70.0f) {
        temp_color = lv_color_hex(0xFF4444); /* Bright Red */
    } else if (stats->cpu_temp > 60.0f) {
        temp_color = lv_color_hex(0xFF6B6B); /* Orange-Red */
    } else {
        temp_color = lv_color_hex(0x4CAF50); /* Green */
    }
    lv_obj_set_style_text_color(s->label_temp, temp_color, LV_PART_MAIN);
}
