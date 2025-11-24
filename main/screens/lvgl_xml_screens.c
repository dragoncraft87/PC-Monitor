#include <stdio.h>
#include "lvgl.h"
#include "screens.h"
#include "esp_log.h"

static const char *TAG = "LVGL_XML";

// LVGL Display Objekte für jedes Display
static lv_obj_t *cpu_screen = NULL;
static lv_obj_t *gpu_screen = NULL;
static lv_obj_t *ram_screen = NULL;
static lv_obj_t *network_screen = NULL;

// XML Komponenten als Strings (alternativ können diese aus Dateien geladen werden)
#include "lvgl_xml_components.h"

/**
 * Initialisiert die XML-Komponenten
 */
void lvgl_xml_init(void) {
    ESP_LOGI(TAG, "Registering XML components...");

    // Registriere globals.xml (wird automatisch von LVGL gesucht)
    // Falls du globals.xml manuell laden möchtest:
    // lv_xml_register_globals_from_file("A:/main/globals.xml");

    // Registriere die Screen-Komponenten
    lv_xml_register_component_from_file("screen_cpu", "A:/main/screen_cpu.xml");
    lv_xml_register_component_from_file("screen_gpu", "A:/main/screen_gpu.xml");
    lv_xml_register_component_from_file("screen_ram", "A:/main/screen_ram.xml");
    lv_xml_register_component_from_file("screen_network", "A:/main/screen_network.xml");

    ESP_LOGI(TAG, "XML components registered successfully");
}

/**
 * Erstellt die Screens für alle vier Displays
 * HINWEIS: Du musst für jedes Display einen eigenen lv_display_t erstellen
 */
void lvgl_xml_create_screens(lv_display_t *disp_cpu, lv_display_t *disp_gpu,
                              lv_display_t *disp_ram, lv_display_t *disp_network) {
    ESP_LOGI(TAG, "Creating screens from XML...");

    // CPU Screen erstellen
    lv_display_set_default(disp_cpu);
    cpu_screen = (lv_obj_t *)lv_xml_create(lv_screen_active(), "screen_cpu", NULL);

    // GPU Screen erstellen
    lv_display_set_default(disp_gpu);
    gpu_screen = (lv_obj_t *)lv_xml_create(lv_screen_active(), "screen_gpu", NULL);

    // RAM Screen erstellen
    lv_display_set_default(disp_ram);
    ram_screen = (lv_obj_t *)lv_xml_create(lv_screen_active(), "screen_ram", NULL);

    // Network Screen erstellen
    lv_display_set_default(disp_network);
    network_screen = (lv_obj_t *)lv_xml_create(lv_screen_active(), "screen_network", NULL);

    ESP_LOGI(TAG, "All screens created successfully");
}

/**
 * Aktualisiert den CPU-Screen mit neuen Daten
 */
void lvgl_xml_update_cpu(const pc_stats_t *stats) {
    if (cpu_screen == NULL) return;

    // Methode 1: Direkt die Labels finden und Text setzen
    lv_obj_t *cpu_value = lv_obj_get_child_by_id(cpu_screen, "cpu_value");
    if (cpu_value) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", stats->cpu_percent);
        lv_label_set_text(cpu_value, buf);
    }

    lv_obj_t *cpu_bar = lv_obj_get_child_by_id(cpu_screen, "cpu_bar");
    if (cpu_bar) {
        lv_bar_set_value(cpu_bar, stats->cpu_percent, LV_ANIM_ON);
    }

    lv_obj_t *temp_value = lv_obj_get_child_by_id(cpu_screen, "temp_value");
    if (temp_value) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f°C", stats->cpu_temp);
        lv_label_set_text(temp_value, buf);
    }

    // Optional: Farbe je nach Temperatur ändern
    if (stats->cpu_temp > 80) {
        lv_obj_set_style_text_color(temp_value, lv_color_hex(0xFF0000), 0);
    } else if (stats->cpu_temp > 70) {
        lv_obj_set_style_text_color(temp_value, lv_color_hex(0xFFAA00), 0);
    } else {
        lv_obj_set_style_text_color(temp_value, lv_color_hex(0xFFFFFF), 0);
    }
}

/**
 * Aktualisiert den GPU-Screen mit neuen Daten
 */
void lvgl_xml_update_gpu(const pc_stats_t *stats) {
    if (gpu_screen == NULL) return;

    lv_obj_t *gpu_value = lv_obj_get_child_by_id(gpu_screen, "gpu_value");
    if (gpu_value) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", stats->gpu_percent);
        lv_label_set_text(gpu_value, buf);
    }

    lv_obj_t *gpu_bar = lv_obj_get_child_by_id(gpu_screen, "gpu_bar");
    if (gpu_bar) {
        lv_bar_set_value(gpu_bar, stats->gpu_percent, LV_ANIM_ON);
    }

    lv_obj_t *temp_value = lv_obj_get_child_by_id(gpu_screen, "temp_value");
    if (temp_value) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f°C", stats->gpu_temp);
        lv_label_set_text(temp_value, buf);
    }

    lv_obj_t *vram_value = lv_obj_get_child_by_id(gpu_screen, "vram_value");
    if (vram_value) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f/%.1f GB", stats->gpu_vram_used, stats->gpu_vram_total);
        lv_label_set_text(vram_value, buf);
    }
}

/**
 * Aktualisiert den RAM-Screen mit neuen Daten
 */
void lvgl_xml_update_ram(const pc_stats_t *stats) {
    if (ram_screen == NULL) return;

    int ram_percent = (int)((stats->ram_used_gb / stats->ram_total_gb) * 100);

    lv_obj_t *ram_percent_value = lv_obj_get_child_by_id(ram_screen, "ram_percent_value");
    if (ram_percent_value) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", ram_percent);
        lv_label_set_text(ram_percent_value, buf);
    }

    lv_obj_t *ram_bar = lv_obj_get_child_by_id(ram_screen, "ram_bar");
    if (ram_bar) {
        lv_bar_set_value(ram_bar, ram_percent, LV_ANIM_ON);
    }

    lv_obj_t *ram_value = lv_obj_get_child_by_id(ram_screen, "ram_value");
    if (ram_value) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f / %.1f GB", stats->ram_used_gb, stats->ram_total_gb);
        lv_label_set_text(ram_value, buf);
    }
}

/**
 * Aktualisiert den Network-Screen mit neuen Daten
 */
void lvgl_xml_update_network(const pc_stats_t *stats) {
    if (network_screen == NULL) return;

    lv_obj_t *net_type_value = lv_obj_get_child_by_id(network_screen, "net_type_value");
    if (net_type_value) {
        lv_label_set_text(net_type_value, stats->net_type);
    }

    lv_obj_t *speed_value = lv_obj_get_child_by_id(network_screen, "speed_value");
    if (speed_value) {
        lv_label_set_text(speed_value, stats->net_speed);
    }

    lv_obj_t *down_value = lv_obj_get_child_by_id(network_screen, "down_value");
    if (down_value) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f MB/s", stats->net_down_mbps);
        lv_label_set_text(down_value, buf);
    }

    lv_obj_t *up_value = lv_obj_get_child_by_id(network_screen, "up_value");
    if (up_value) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f MB/s", stats->net_up_mbps);
        lv_label_set_text(up_value, buf);
    }
}

/**
 * Aktualisiert alle Screens mit neuen Daten
 */
void lvgl_xml_update_all(const pc_stats_t *stats) {
    lvgl_xml_update_cpu(stats);
    lvgl_xml_update_gpu(stats);
    lvgl_xml_update_ram(stats);
    lvgl_xml_update_network(stats);
}
