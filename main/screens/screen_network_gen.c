/**
 * @file screen_network_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_network_gen.h"
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

lv_obj_t * screen_network_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    static bool style_inited = false;

    if (!style_inited) {

        style_inited = true;
    }

    lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
    lv_obj_set_name_static(lv_obj_0, "screen_network_#");
    lv_obj_set_width(lv_obj_0, 240);
    lv_obj_set_height(lv_obj_0, 240);
    lv_obj_set_style_bg_color(lv_obj_0, BG_DARK, 0);
    lv_obj_set_style_bg_opa(lv_obj_0, 255, 0);
    lv_obj_set_style_border_width(lv_obj_0, 0, 0);
    lv_obj_set_style_radius(lv_obj_0, 120, 0);

    lv_obj_t * lv_label_0 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_0, "NETWORK");
    lv_obj_set_x(lv_label_0, 0);
    lv_obj_set_y(lv_label_0, 20);
    lv_obj_set_width(lv_label_0, 240);
    lv_obj_set_align(lv_label_0, LV_ALIGN_TOP_MID);
    lv_obj_set_style_text_color(lv_label_0, TEXT_WHITE, 0);
    lv_obj_set_style_text_align(lv_label_0, LV_TEXT_ALIGN_CENTER, 0);
    
    lv_obj_t * lv_label_1 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_1, "WiFi");
    lv_obj_set_x(lv_label_1, 0);
    lv_obj_set_y(lv_label_1, 60);
    lv_obj_set_width(lv_label_1, 240);
    lv_obj_set_align(lv_label_1, LV_ALIGN_TOP_MID);
    lv_obj_set_style_text_color(lv_label_1, NET_COLOR, 0);
    lv_obj_set_style_text_align(lv_label_1, LV_TEXT_ALIGN_CENTER, 0);
    
    lv_obj_t * lv_label_2 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_2, "Speed");
    lv_obj_set_x(lv_label_2, 0);
    lv_obj_set_y(lv_label_2, 100);
    lv_obj_set_width(lv_label_2, 240);
    lv_obj_set_align(lv_label_2, LV_ALIGN_TOP_MID);
    lv_obj_set_style_text_color(lv_label_2, TEXT_GRAY, 0);
    lv_obj_set_style_text_align(lv_label_2, LV_TEXT_ALIGN_CENTER, 0);
    
    lv_obj_t * lv_label_3 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_3, "1000 Mbps");
    lv_obj_set_x(lv_label_3, 0);
    lv_obj_set_y(lv_label_3, 120);
    lv_obj_set_width(lv_label_3, 240);
    lv_obj_set_align(lv_label_3, LV_ALIGN_TOP_MID);
    lv_obj_set_style_text_color(lv_label_3, TEXT_WHITE, 0);
    lv_obj_set_style_text_align(lv_label_3, LV_TEXT_ALIGN_CENTER, 0);
    
    lv_obj_t * lv_label_4 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_4, "Download");
    lv_obj_set_x(lv_label_4, -50);
    lv_obj_set_y(lv_label_4, 160);
    lv_obj_set_width(lv_label_4, 100);
    lv_obj_set_align(lv_label_4, LV_ALIGN_TOP_MID);
    lv_obj_set_style_text_color(lv_label_4, TEXT_GRAY, 0);
    lv_obj_set_style_text_align(lv_label_4, LV_TEXT_ALIGN_CENTER, 0);
    
    lv_obj_t * lv_label_5 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_5, "25.4 MB/s");
    lv_obj_set_x(lv_label_5, -50);
    lv_obj_set_y(lv_label_5, 180);
    lv_obj_set_width(lv_label_5, 100);
    lv_obj_set_align(lv_label_5, LV_ALIGN_TOP_MID);
    lv_obj_set_style_text_color(lv_label_5, NET_COLOR, 0);
    lv_obj_set_style_text_align(lv_label_5, LV_TEXT_ALIGN_CENTER, 0);
    
    lv_obj_t * lv_label_6 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_6, "Upload");
    lv_obj_set_x(lv_label_6, 50);
    lv_obj_set_y(lv_label_6, 160);
    lv_obj_set_width(lv_label_6, 100);
    lv_obj_set_align(lv_label_6, LV_ALIGN_TOP_MID);
    lv_obj_set_style_text_color(lv_label_6, TEXT_GRAY, 0);
    lv_obj_set_style_text_align(lv_label_6, LV_TEXT_ALIGN_CENTER, 0);
    
    lv_obj_t * lv_label_7 = lv_label_create(lv_obj_0);
    lv_label_set_text(lv_label_7, "5.8 MB/s");
    lv_obj_set_x(lv_label_7, 50);
    lv_obj_set_y(lv_label_7, 180);
    lv_obj_set_width(lv_label_7, 100);
    lv_obj_set_align(lv_label_7, LV_ALIGN_TOP_MID);
    lv_obj_set_style_text_color(lv_label_7, NET_COLOR, 0);
    lv_obj_set_style_text_align(lv_label_7, LV_TEXT_ALIGN_CENTER, 0);

    LV_TRACE_OBJ_CREATE("finished");

    return lv_obj_0;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

