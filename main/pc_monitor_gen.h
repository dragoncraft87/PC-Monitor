/**
 * @file pc_monitor_gen.h
 */

#ifndef PC_MONITOR_GEN_H
#define PC_MONITOR_GEN_H

#ifndef UI_SUBJECT_STRING_LENGTH
#define UI_SUBJECT_STRING_LENGTH 256
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl.h"
#else
    #include "lvgl/lvgl.h"
#endif

/*********************
 *      DEFINES
 *********************/

#define BG_DARK lv_color_hex(0x000000)

#define BG_GRAY lv_color_hex(0x202020)

#define TEXT_WHITE lv_color_hex(0xFFFFFF)

#define TEXT_GRAY lv_color_hex(0xAAAAAA)

#define CPU_COLOR lv_color_hex(0x00AAFF)

#define CPU_WARNING lv_color_hex(0xFFAA00)

#define CPU_CRITICAL lv_color_hex(0xFF0000)

#define GPU_COLOR lv_color_hex(0x00FF66)

#define RAM_COLOR lv_color_hex(0xFF6600)

#define NET_COLOR lv_color_hex(0xAA00FF)

#define DISPLAY_WIDTH 240

#define DISPLAY_HEIGHT 240

#define PADDING_STD 10

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL VARIABLES
 **********************/

/*-------------------
 * Permanent screens
 *------------------*/

/*----------------
 * Global styles
 *----------------*/

extern lv_style_t round_Display;
extern lv_style_t screen_bg;
extern lv_style_t title_style;
extern lv_style_t value_large_style;
extern lv_style_t value_small_style;
extern lv_style_t bar_bg_style;

/*----------------
 * Fonts
 *----------------*/

extern lv_font_t * montserrat_14;

extern lv_font_t * montserrat_20;

extern lv_font_t * montserrat_48;

/*----------------
 * Images
 *----------------*/

/*----------------
 * Subjects
 *----------------*/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/*----------------
 * Event Callbacks
 *----------------*/

/**
 * Initialize the component library
 */

void pc_monitor_init_gen(const char * asset_path);

/**********************
 *      MACROS
 **********************/

/**********************
 *   POST INCLUDES
 **********************/

/*Include all the widget and components of this library*/
#include "screens/screen_cpu_gen.h"
#include "screens/screen_gpu_gen.h"
#include "screens/screen_network_gen.h"
#include "screens/screen_ram_gen.h"

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*PC_MONITOR_GEN_H*/