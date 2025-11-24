#ifndef LVGL_XML_SCREENS_H
#define LVGL_XML_SCREENS_H

#include "lvgl.h"
#include "screens.h"

/**
 * Initialisiert die XML-Komponenten und registriert sie in LVGL
 */
void lvgl_xml_init(void);

/**
 * Erstellt die Screens für alle vier Displays aus XML
 *
 * @param disp_cpu      Display-Objekt für CPU-Screen
 * @param disp_gpu      Display-Objekt für GPU-Screen
 * @param disp_ram      Display-Objekt für RAM-Screen
 * @param disp_network  Display-Objekt für Network-Screen
 */
void lvgl_xml_create_screens(lv_display_t *disp_cpu, lv_display_t *disp_gpu,
                              lv_display_t *disp_ram, lv_display_t *disp_network);

/**
 * Aktualisiert den CPU-Screen mit neuen Daten
 * @param stats PC-Statistiken
 */
void lvgl_xml_update_cpu(const pc_stats_t *stats);

/**
 * Aktualisiert den GPU-Screen mit neuen Daten
 * @param stats PC-Statistiken
 */
void lvgl_xml_update_gpu(const pc_stats_t *stats);

/**
 * Aktualisiert den RAM-Screen mit neuen Daten
 * @param stats PC-Statistiken
 */
void lvgl_xml_update_ram(const pc_stats_t *stats);

/**
 * Aktualisiert den Network-Screen mit neuen Daten
 * @param stats PC-Statistiken
 */
void lvgl_xml_update_network(const pc_stats_t *stats);

/**
 * Aktualisiert alle Screens mit neuen Daten
 * @param stats PC-Statistiken
 */
void lvgl_xml_update_all(const pc_stats_t *stats);

#endif // LVGL_XML_SCREENS_H
