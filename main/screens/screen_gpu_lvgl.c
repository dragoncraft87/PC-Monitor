/**
 * @file screen_gpu_lvgl.c
 * @brief GPU Gauge Screen (Display 2) - LVGL Implementation
 *
 * Design based on gpu-gauge.html:
 * - Ring gauge (Arc widget) showing GPU percentage
 * - Gradient: Cyan (#4cc9f0) → Blue (#4361ee)
 * - Center text: "GPU", percentage, temperature, VRAM
 */

#include "screens_lvgl.h"
#include <stdio.h>

struct screen_gpu_t {
    lv_obj_t *screen;
    lv_obj_t *arc;
    lv_obj_t *label_title;
    lv_obj_t *label_percent;
    lv_obj_t *label_temp;
    lv_obj_t *label_vram;
};

screen_gpu_t *screen_gpu_create(lv_display_t *disp)
{
    screen_gpu_t *s = malloc(sizeof(screen_gpu_t));
    if (!s) return NULL;

    /* Set this display as default temporarily */
    lv_display_t *old_default = lv_display_get_default();
    lv_display_set_default(disp);

    /* Create screen */
    s->screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s->screen, lv_color_black(), 0);

    /* ========================================================================
     * ARC WIDGET (Ring Gauge)
     * ====================================================================== */
    s->arc = lv_arc_create(s->screen);
    lv_obj_set_size(s->arc, 200, 200);
    lv_obj_center(s->arc);

    lv_arc_set_range(s->arc, 0, 100);
    lv_arc_set_value(s->arc, 0);
    lv_arc_set_bg_angles(s->arc, 135, 45);
    lv_arc_set_rotation(s->arc, 0);

    /* Styling - NVIDIA Green (matching Intel Blue style) */
    lv_obj_set_style_arc_width(s->arc, 20, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s->arc, 20, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s->arc, lv_color_hex(0x55555C), LV_PART_MAIN);
    lv_obj_set_style_arc_color(s->arc, lv_color_hex(0x76B900), LV_PART_INDICATOR);  /* NVIDIA Green */
    lv_obj_set_style_arc_rounded(s->arc, false, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(s->arc, false, LV_PART_INDICATOR);

    lv_obj_set_style_bg_opa(s->arc, 0, LV_PART_KNOB);

    /* ========================================================================
     * CENTER LABELS
     * ====================================================================== */

    /* GPU Model Title */
    s->label_title = lv_label_create(s->screen);
    lv_label_set_text(s->label_title, "RTX 3080 Ti");
    lv_obj_set_style_text_font(s->label_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s->label_title, lv_color_hex(0xCCCCCC), 0);
    lv_obj_align(s->label_title, LV_ALIGN_CENTER, 0, -50);

    /* Percentage Value - Center Top */
    s->label_percent = lv_label_create(s->screen);
    lv_label_set_text(s->label_percent, "0%");
    lv_obj_set_style_text_font(s->label_percent, &lv_font_montserrat_42, 0);
    lv_obj_set_style_text_color(s->label_percent, lv_color_white(), 0);
    lv_obj_align(s->label_percent, LV_ALIGN_CENTER, 0, -8);

    /* VRAM - Center (bigger font, more space) */
    s->label_vram = lv_label_create(s->screen);
    lv_label_set_text(s->label_vram, "0/12GB");
    lv_obj_set_style_text_font(s->label_vram, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s->label_vram, lv_color_hex(0x4CAF50), 0);
    lv_obj_align(s->label_vram, LV_ALIGN_CENTER, 0, 30);

    /* Temperature - Bottom */
    s->label_temp = lv_label_create(s->screen);
    lv_label_set_text(s->label_temp, "0C");
    lv_obj_set_style_text_font(s->label_temp, &lv_font_montserrat_34, 0);
    lv_obj_set_style_text_color(s->label_temp, lv_color_hex(0xFF6B6B), 0);
    lv_obj_align(s->label_temp, LV_ALIGN_CENTER, 0, 70);

    /* Load screen to this display */
    lv_screen_load(s->screen);

    /* Restore previous default display */
    if (old_default) {
        lv_display_set_default(old_default);
    }

    return s;
}

void screen_gpu_update(screen_gpu_t *s, const pc_stats_t *stats)
{
    if (!s) return;

    /* Update arc */
    lv_arc_set_value(s->arc, stats->gpu_percent);

    /* Update percentage */
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", stats->gpu_percent);
    lv_label_set_text(s->label_percent, buf);

    /* Update VRAM (12GB total) */
    char vram_buf[16];
    snprintf(vram_buf, sizeof(vram_buf), "%.1f/12GB", stats->gpu_vram_used);
    lv_label_set_text(s->label_vram, vram_buf);

    /* Update temperature (no decimals) */
    char temp_buf[16];
    snprintf(temp_buf, sizeof(temp_buf), "%d°C", (int)stats->gpu_temp);
    lv_label_set_text(s->label_temp, temp_buf);

    /* Temperature color */
    lv_color_t temp_color;
    if (stats->gpu_temp > 75.0f) {
        temp_color = lv_color_hex(0xFF4444);
    } else if (stats->gpu_temp > 65.0f) {
        temp_color = lv_color_hex(0xFF6B6B);
    } else {
        temp_color = lv_color_hex(0x4CAF50);
    }
    lv_obj_set_style_text_color(s->label_temp, temp_color, 0);
}
