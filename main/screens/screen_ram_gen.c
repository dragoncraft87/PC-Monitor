/**
 * @file screen_ram_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_ram_gen.h"
#include "pc_monitor.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/***********************
 *  STATIC VARIABLES
 **********************/

/***********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * screen_ram_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    static bool style_inited = false;

    if (!style_inited) {

        style_inited = true;
    }

    lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
    lv_obj_set_name_static(lv_obj_0, "screen_ram_#");
    lv_obj_set_width(lv_obj_0, 240);
    lv_obj_set_height(lv_obj_0, 240);
    lv_obj_set_style_bg_color(lv_obj_0, BG_DARK, 0);
    lv_obj_set_style_bg_opa(lv_obj_0, 255, 0);
    lv_obj_set_style_border_width(lv_obj_0, 0, 0);
    lv_obj_set_style_radius(lv_obj_0, 120, 0);

    lv_obj_t * lv_arc_0 = lv_arc_create(lv_obj_0);
    lv_obj_set_x(lv_arc_0, 0);
    lv_obj_set_y(lv_arc_0, 0);
    lv_obj_set_width(lv_arc_0, 200);
    lv_obj_set_height(lv_arc_0, 200);
    lv_obj_set_align(lv_arc_0, LV_ALIGN_CENTER);
    lv_arc_set_value(lv_arc_0, 68);
    lv_arc_set_rotation(lv_arc_0, 0);
    lv_arc_set_bg_start_angle(lv_arc_0, 0);
    lv_arc_set_bg_end_angle(lv_arc_0, 360);
    lv_arc_set_start_angle(lv_arc_0, 0);
    lv_arc_set_end_angle(lv_arc_0, 360);
    lv_arc_set_mode(lv_arc_0, LV_ARC_MODE_NORMAL);
    lv_obj_set_style_arc_width(lv_arc_0, 18, 0);
    lv_obj_set_style_arc_color(lv_arc_0, BG_GRAY, 0);
    lv_obj_set_style_arc_opa(lv_arc_0, 255, 0);
    lv_obj_set_style_arc_rounded(lv_arc_0, true, 0);
    
    lv_obj_t * lv_label_0 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_0, "RAM");
    lv_obj_set_x(lv_label_0, 0);
    lv_obj_set_y(lv_label_0, -35);
    lv_obj_set_width(lv_label_0, 240);
    lv_obj_set_align(lv_label_0, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(lv_label_0, TEXT_GRAY, 0);
    lv_obj_set_style_text_align(lv_label_0, LV_TEXT_ALIGN_CENTER, 0);
    
    lv_obj_t * lv_label_1 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_1, "68%");
    lv_obj_set_x(lv_label_1, 0);
    lv_obj_set_y(lv_label_1, 0);
    lv_obj_set_width(lv_label_1, 240);
    lv_obj_set_align(lv_label_1, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(lv_label_1, RAM_COLOR, 0);
    lv_obj_set_style_text_align(lv_label_1, LV_TEXT_ALIGN_CENTER, 0);
    
    lv_obj_t * lv_label_2 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_2, "10.9 / 16.0 GB");
    lv_obj_set_x(lv_label_2, 0);
    lv_obj_set_y(lv_label_2, 35);
    lv_obj_set_width(lv_label_2, 240);
    lv_obj_set_align(lv_label_2, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(lv_label_2, TEXT_WHITE, 0);
    lv_obj_set_style_text_align(lv_label_2, LV_TEXT_ALIGN_CENTER, 0);

    LV_TRACE_OBJ_CREATE("finished");

    return lv_obj_0;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

