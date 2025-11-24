/**
 * BEISPIEL: Integration von LVGL mit XML in main.c
 *
 * Dies ist ein Beispiel, wie du LVGL mit XML in deinem PC-Monitor-Projekt verwenden kannst.
 * Du kannst diese Funktion in deine main.c integrieren oder als Referenz verwenden.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "lvgl.h"
#include "gc9a01.h"
#include "screens.h"
#include "lvgl_xml_screens.h"

static const char *TAG = "MAIN_LVGL_EXAMPLE";

// LVGL Display Objekte
static lv_display_t *lvgl_disp_cpu = NULL;
static lv_display_t *lvgl_disp_gpu = NULL;
static lv_display_t *lvgl_disp_ram = NULL;
static lv_display_t *lvgl_disp_network = NULL;

// Display handles (deine bestehenden)
static gc9a01_handle_t display_cpu;
static gc9a01_handle_t display_gpu;
static gc9a01_handle_t display_ram;
static gc9a01_handle_t display_network;

// PC stats
static pc_stats_t pc_stats = {
    .cpu_percent = 45,
    .cpu_temp = 62.5,
    .gpu_percent = 72,
    .gpu_temp = 68.3,
    .gpu_vram_used = 4.2,
    .gpu_vram_total = 8.0,
    .ram_used_gb = 10.4,
    .ram_total_gb = 16.0,
    .net_type = "LAN",
    .net_speed = "1000 Mbps",
    .net_down_mbps = 12.4,
    .net_up_mbps = 2.1
};

/**
 * Flush Callback für LVGL - schreibt auf das GC9A01 Display
 */
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    gc9a01_handle_t *display = (gc9a01_handle_t *)lv_display_get_user_data(disp);

    uint16_t width = area->x2 - area->x1 + 1;
    uint16_t height = area->y2 - area->y1 + 1;

    // Schreibe Pixel auf Display
    // HINWEIS: Du musst diese Funktion an deine gc9a01-Implementierung anpassen
    // gc9a01_write_pixels(display, area->x1, area->y1, width, height, (uint16_t *)px_map);

    lv_display_flush_ready(disp);
}

/**
 * LVGL Timer Task - muss regelmäßig aufgerufen werden
 */
static void lvgl_timer_task(void *arg)
{
    ESP_LOGI(TAG, "LVGL timer task started");

    while (1) {
        // LVGL Timer Handler - muss alle paar ms aufgerufen werden
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * Initialisiert ein LVGL Display für ein GC9A01
 */
static lv_display_t *lvgl_init_display(gc9a01_handle_t *gc_display)
{
    // Buffer für LVGL erstellen (anpassen je nach verfügbarem RAM)
    static lv_color_t buf1[240 * 10]; // 10 Zeilen Buffer
    static lv_color_t buf2[240 * 10]; // Double buffering (optional)

    lv_display_t *disp = lv_display_create(240, 240);
    lv_display_set_flush_cb(disp, lvgl_flush_cb);
    lv_display_set_buffers(disp, buf1, buf2, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_user_data(disp, gc_display);

    return disp;
}

/**
 * Beispiel-Funktion zum Initialisieren von LVGL mit XML
 */
void example_init_lvgl_with_xml(void)
{
    ESP_LOGI(TAG, "Initializing LVGL...");

    // 1. LVGL initialisieren
    lv_init();

    // 2. Displays für LVGL erstellen
    lvgl_disp_cpu = lvgl_init_display(&display_cpu);
    lvgl_disp_gpu = lvgl_init_display(&display_gpu);
    lvgl_disp_ram = lvgl_init_display(&display_ram);
    lvgl_disp_network = lvgl_init_display(&display_network);

    ESP_LOGI(TAG, "LVGL displays created");

    // 3. XML-Komponenten registrieren
    lvgl_xml_init();

    // 4. Screens aus XML erstellen
    lvgl_xml_create_screens(lvgl_disp_cpu, lvgl_disp_gpu,
                            lvgl_disp_ram, lvgl_disp_network);

    ESP_LOGI(TAG, "XML screens created");

    // 5. LVGL Timer Task starten
    xTaskCreate(lvgl_timer_task, "lvgl_timer", 4096, NULL, 10, NULL);

    ESP_LOGI(TAG, "LVGL initialization complete");
}

/**
 * Beispiel Update-Task für LVGL
 */
static void lvgl_display_update_task(void *arg)
{
    ESP_LOGI(TAG, "LVGL display update task started");

    while (1) {
        // Aktualisiere alle Screens mit den aktuellen Daten
        lvgl_xml_update_all(&pc_stats);

        // Warte 1 Sekunde bis zum nächsten Update
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
 * Alternative Integration in app_main()
 *
 * Ersetze in deiner main.c die bestehenden Aufrufe durch:
 */
void example_app_main_integration(void)
{
    ESP_LOGI(TAG, "PC Monitor with LVGL+XML starting...");

    // USB Serial und SPI initialisieren (wie vorher)
    // ...

    // GC9A01 Displays initialisieren (wie vorher)
    // gc9a01_init(&display_cpu, &pins_cpu, SPI2_HOST);
    // gc9a01_init(&display_gpu, &pins_gpu, SPI2_HOST);
    // gc9a01_init(&display_ram, &pins_ram, SPI2_HOST);
    // gc9a01_init(&display_network, &pins_network, SPI2_HOST);

    // LVGL mit XML initialisieren (NEU)
    example_init_lvgl_with_xml();

    // Tasks erstellen
    // xTaskCreate(usb_rx_task, "usb_rx", 4096, NULL, 10, NULL);
    xTaskCreate(lvgl_display_update_task, "lvgl_update", 8192, NULL, 5, NULL);

    ESP_LOGI(TAG, "System ready!");
}

/*
 * HINWEISE:
 *
 * 1. LVGL_KCONFIG aktivieren:
 *    - Führe 'idf.py menuconfig' aus
 *    - Navigiere zu "Component config" -> "LVGL configuration"
 *    - Aktiviere "Enable XML support" (LV_USE_XML)
 *
 * 2. XML-Dateien müssen im Dateisystem verfügbar sein:
 *    - Entweder über SPIFFS/LittleFS
 *    - Oder als embedded files im Flash
 *    - Die Pfade in lvgl_xml_screens.c anpassen ("A:/main/..." für SPIFFS)
 *
 * 3. Buffer-Größe anpassen:
 *    - Je nach verfügbarem RAM kannst du größere Buffer verwenden
 *    - Größere Buffer = schnelleres Rendering
 *
 * 4. Flush-Callback anpassen:
 *    - Die lvgl_flush_cb Funktion muss an deine gc9a01-Implementierung
 *      angepasst werden
 *
 * 5. CMakeLists.txt anpassen:
 *    - "lvgl_xml_screens.c" zur SRCS-Liste hinzufügen
 */
