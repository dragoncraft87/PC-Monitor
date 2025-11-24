/**
 * @file pc_monitor_gen.c
 */

/*********************
 *      INCLUDES
 *********************/

#include "pc_monitor_gen.h"

#if LV_USE_XML
#endif /* LV_USE_XML */

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/*----------------
 * Translations
 *----------------*/

/**********************
 *  GLOBAL VARIABLES
 **********************/

/*--------------------
 *  Permanent screens
 *-------------------*/

/*----------------
 * Global styles
 *----------------*/

lv_style_t round_Display;
lv_style_t screen_bg;
lv_style_t title_style;
lv_style_t value_large_style;
lv_style_t value_small_style;
lv_style_t bar_bg_style;

/*----------------
 * Fonts
 *----------------*/

lv_font_t * montserrat_14;
lv_font_t * montserrat_20;
lv_font_t * montserrat_48;

/*----------------
 * Images
 *----------------*/

/*----------------
 * Subjects
 *----------------*/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void pc_monitor_init_gen(const char * asset_path)
{
    char buf[256];

    /*----------------
     * Global styles
     *----------------*/

    static bool style_inited = false;

    if (!style_inited) {
        lv_style_init(&round_Display);
        lv_style_set_radius(&round_Display, 120);

        lv_style_init(&screen_bg);
        lv_style_set_bg_color(&screen_bg, BG_DARK);
        lv_style_set_bg_opa(&screen_bg, 255);
        lv_style_set_border_width(&screen_bg, 0);

        lv_style_init(&title_style);
        lv_style_set_text_color(&title_style, TEXT_WHITE);
        lv_style_set_text_font(&title_style, montserrat_20);
        lv_style_set_text_align(&title_style, LV_TEXT_ALIGN_CENTER);

        lv_style_init(&value_large_style);
        lv_style_set_text_color(&value_large_style, TEXT_WHITE);
        lv_style_set_text_font(&value_large_style, montserrat_48);
        lv_style_set_text_align(&value_large_style, LV_TEXT_ALIGN_CENTER);

        lv_style_init(&value_small_style);
        lv_style_set_text_color(&value_small_style, TEXT_GRAY);
        lv_style_set_text_font(&value_small_style, montserrat_14);
        lv_style_set_text_align(&value_small_style, LV_TEXT_ALIGN_CENTER);

        lv_style_init(&bar_bg_style);
        lv_style_set_bg_color(&bar_bg_style, BG_GRAY);
        lv_style_set_bg_opa(&bar_bg_style, 255);
        lv_style_set_radius(&bar_bg_style, 5);

        style_inited = true;
    }

    /*----------------
     * Fonts
     *----------------*/

    /* create tiny ttf font "montserrat_14" from file */
    lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Montserrat/Montserrat-Medium.ttf");
    montserrat_14 = lv_tiny_ttf_create_file(buf, 14);
    /* create tiny ttf font "montserrat_20" from file */
    lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Montserrat/Montserrat-Medium.ttf");
    montserrat_20 = lv_tiny_ttf_create_file(buf, 20);
    /* create tiny ttf font "montserrat_48" from file */
    lv_snprintf(buf, 256, "%s%s", asset_path, "fonts/Montserrat/Montserrat-Medium.ttf");
    montserrat_48 = lv_tiny_ttf_create_file(buf, 48);


    /*----------------
     * Images
     *----------------*/
    /*----------------
     * Subjects
     *----------------*/
    /*----------------
     * Translations
     *----------------*/

#if LV_USE_XML
    /* Register widgets */

    /* Register fonts */
    lv_xml_register_font(NULL, "montserrat_14", montserrat_14);
    lv_xml_register_font(NULL, "montserrat_20", montserrat_20);
    lv_xml_register_font(NULL, "montserrat_48", montserrat_48);

    /* Register subjects */

    /* Register callbacks */
#endif

    /* Register all the global assets so that they won't be created again when globals.xml is parsed.
     * While running in the editor skip this step to update the preview when the XML changes */
#if LV_USE_XML && !defined(LV_EDITOR_PREVIEW)
    /* Register images */
#endif

#if LV_USE_XML == 0
    /*--------------------
     *  Permanent screens
     *-------------------*/
    /* If XML is enabled it's assumed that the permanent screens are created
     * manaully from XML using lv_xml_create() */
#endif
}

/* Callbacks */

/**********************
 *   STATIC FUNCTIONS
 **********************/