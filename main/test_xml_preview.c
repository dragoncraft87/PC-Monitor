/**
 * XML Preview Test für ESP32
 *
 * Dieses Programm zeigt einen einzelnen Screen zur Vorschau an.
 * Nutze es um die XML-Screens zu testen, bevor du sie in main.c integrierst.
 *
 * Kompiliere mit: idf.py build flash monitor
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "lvgl.h"
#include "gc9a01.h"

static const char *TAG = "XML_PREVIEW";

// Display handle
static gc9a01_handle_t display;
static lv_display_t *lvgl_disp = NULL;

// GPIO Pins für ein Display (CPU Display als Test)
static const gc9a01_pins_t pins = {
    .sck = 4,
    .mosi = 5,
    .cs = 11,
    .dc = 12,
    .rst = 13
};

// Flush Callback für LVGL
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    gc9a01_handle_t *gc_disp = (gc9a01_handle_t *)lv_display_get_user_data(disp);

    // Berechne Breite und Höhe
    uint16_t width = area->x2 - area->x1 + 1;
    uint16_t height = area->y2 - area->y1 + 1;

    // WICHTIG: Diese Funktion musst du in gc9a01.c implementieren
    // Sie sollte einen Pixel-Buffer an eine bestimmte Position schreiben
    //
    // Beispiel-Signatur:
    // void gc9a01_draw_bitmap(gc9a01_handle_t *display, uint16_t x, uint16_t y,
    //                         uint16_t w, uint16_t h, const uint16_t *data);
    //
    // gc9a01_draw_bitmap(gc_disp, area->x1, area->y1, width, height, (uint16_t *)px_map);

    // Temporäre Lösung: Pixel für Pixel schreiben (langsam!)
    for (int y = area->y1; y <= area->y2; y++) {
        for (int x = area->x1; x <= area->x2; x++) {
            // Hole Pixel-Farbe aus Buffer
            int idx = ((y - area->y1) * width + (x - area->x1)) * 2;
            uint16_t color = (px_map[idx + 1] << 8) | px_map[idx];

            // Schreibe Pixel (ineffizient, aber funktioniert)
            // gc9a01_draw_pixel(gc_disp, x, y, color);
        }
    }

    // Signalisiere LVGL, dass Flush fertig ist
    lv_display_flush_ready(disp);
}

// LVGL Timer Task
static void lvgl_timer_task(void *arg)
{
    ESP_LOGI(TAG, "LVGL timer task started");
    while (1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// XML-Screens registrieren
static void register_xml_screens(void)
{
    ESP_LOGI(TAG, "Registering XML components...");

    // OPTION 1: Aus Dateien laden (benötigt SPIFFS/LittleFS)
    /*
    lv_xml_register_globals_from_file("A:/main/globals.xml");
    lv_xml_register_component_from_file("screen_cpu", "A:/main/screens/screen_cpu.xml");
    */

    // OPTION 2: Embedded XML (empfohlen für Test)
    // Füge in CMakeLists.txt hinzu:
    //   EMBED_FILES "globals.xml" "screens/screen_cpu.xml"

    extern const char globals_xml_start[] asm("_binary_globals_xml_start");
    extern const char globals_xml_end[]   asm("_binary_globals_xml_end");
    extern const char screen_cpu_xml_start[] asm("_binary_screen_cpu_xml_start");
    extern const char screen_cpu_xml_end[]   asm("_binary_screen_cpu_xml_end");

    // Registriere globals
    lv_xml_register_globals_from_data(globals_xml_start);
    ESP_LOGI(TAG, "Globals registered");

    // Registriere screen_cpu
    lv_xml_register_component_from_data("screen_cpu", screen_cpu_xml_start);
    ESP_LOGI(TAG, "screen_cpu registered");
}

// Screen erstellen und anzeigen
static void create_test_screen(void)
{
    ESP_LOGI(TAG, "Creating test screen...");

    // Testdaten für CPU-Screen
    const char *attrs[] = {
        "cpu_percent", "75",
        "cpu_temp", "65.5",
        NULL, NULL
    };

    // Screen erstellen
    lv_obj_t *screen = (lv_obj_t *)lv_xml_create(lv_screen_active(), "screen_cpu", attrs);

    if (screen == NULL) {
        ESP_LOGE(TAG, "Failed to create screen from XML!");
        return;
    }

    ESP_LOGI(TAG, "Screen created successfully");

    // Optional: Werte dynamisch aktualisieren (nach 3 Sekunden)
    vTaskDelay(pdMS_TO_TICKS(3000));

    ESP_LOGI(TAG, "Updating values...");

    // CPU-Wert aktualisieren
    lv_obj_t *cpu_value = lv_obj_get_child_by_id(screen, "cpu_value");
    if (cpu_value) {
        lv_label_set_text(cpu_value, "90%");
        ESP_LOGI(TAG, "CPU value updated");
    }

    // Bar aktualisieren
    lv_obj_t *cpu_bar = lv_obj_get_child_by_id(screen, "cpu_bar");
    if (cpu_bar) {
        lv_bar_set_value(cpu_bar, 90, LV_ANIM_ON);
        ESP_LOGI(TAG, "CPU bar updated");
    }

    // Temperatur aktualisieren
    lv_obj_t *temp_value = lv_obj_get_child_by_id(screen, "temp_value");
    if (temp_value) {
        lv_label_set_text(temp_value, "82.3°C");
        lv_obj_set_style_text_color(temp_value, lv_color_hex(0xFF0000), 0); // Rot bei hoher Temp
        ESP_LOGI(TAG, "Temperature updated");
    }
}

// Animations-Task (optional - zeigt dynamische Updates)
static void animation_task(void *arg)
{
    ESP_LOGI(TAG, "Animation task started");
    vTaskDelay(pdMS_TO_TICKS(5000)); // Warte 5 Sekunden

    lv_obj_t *screen = lv_screen_active();
    int cpu = 50;
    int direction = 1;

    while (1) {
        // Simuliere CPU-Last von 30% bis 95%
        cpu += direction * 5;
        if (cpu >= 95) direction = -1;
        if (cpu <= 30) direction = 1;

        // Update UI
        lv_obj_t *cpu_value = lv_obj_get_child_by_id(screen, "cpu_value");
        lv_obj_t *cpu_bar = lv_obj_get_child_by_id(screen, "cpu_bar");

        if (cpu_value) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d%%", cpu);
            lv_label_set_text(cpu_value, buf);
        }

        if (cpu_bar) {
            lv_bar_set_value(cpu_bar, cpu, LV_ANIM_ON);
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== XML Preview Test ===");

    // 1. SPI initialisieren
    spi_bus_config_t buscfg = {
        .mosi_io_num = 5,
        .miso_io_num = -1,
        .sclk_io_num = 4,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 240 * 240 * 2
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // 2. GC9A01 Display initialisieren
    ESP_LOGI(TAG, "Initializing GC9A01...");
    ESP_ERROR_CHECK(gc9a01_init(&display, &pins, SPI2_HOST));
    gc9a01_fill_screen(&display, 0x0000); // Schwarz
    ESP_LOGI(TAG, "GC9A01 initialized");

    // 3. LVGL initialisieren
    ESP_LOGI(TAG, "Initializing LVGL...");
    lv_init();

    // 4. LVGL Display erstellen
    static lv_color_t buf1[240 * 10]; // Buffer für 10 Zeilen
    static lv_color_t buf2[240 * 10]; // Double buffering

    lvgl_disp = lv_display_create(240, 240);
    lv_display_set_flush_cb(lvgl_disp, lvgl_flush_cb);
    lv_display_set_buffers(lvgl_disp, buf1, buf2, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_user_data(lvgl_disp, &display);
    ESP_LOGI(TAG, "LVGL display created");

    // 5. XML-Komponenten registrieren
    register_xml_screens();

    // 6. Test-Screen erstellen
    create_test_screen();

    // 7. LVGL Timer Task starten
    xTaskCreate(lvgl_timer_task, "lvgl_timer", 4096, NULL, 10, NULL);

    // 8. (Optional) Animations-Task starten
    // xTaskCreate(animation_task, "animation", 2048, NULL, 5, NULL);

    ESP_LOGI(TAG, "=== Preview running ===");
    ESP_LOGI(TAG, "Check your display!");
}

/*
 * WICHTIG: Um dieses Programm zu kompilieren:
 *
 * 1. In CMakeLists.txt:
 *    idf_component_register(
 *        SRCS "test_xml_preview.c" "gc9a01.c"
 *        INCLUDE_DIRS "."
 *        EMBED_FILES "globals.xml" "screens/screen_cpu.xml"
 *    )
 *
 * 2. In sdkconfig oder menuconfig:
 *    - LV_USE_XML=y aktivieren
 *    - Fonts aktivieren (montserrat_14, montserrat_20, montserrat_48)
 *
 * 3. main.c temporär umbenennen:
 *    mv main.c main.c.bak
 *
 * 4. Kompilieren und flashen:
 *    idf.py build flash monitor
 *
 * 5. Wenn fertig, main.c wiederherstellen:
 *    mv main.c.bak main.c
 */
