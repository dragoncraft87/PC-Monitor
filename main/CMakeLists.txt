# Registrierung der Komponente für ESP-IDF Build System
idf_component_register(
    SRCS
        "main_lvgl.c"
        "lvgl_gc9a01_driver.c"
        "screens/screen_cpu_lvgl.c"
        "screens/screen_gpu_lvgl.c"
        "screens/screen_ram_lvgl.c"
        "screens/screen_network_lvgl.c"
    INCLUDE_DIRS
        "."
        "screens"
    REQUIRES
        lvgl
        esp_lcd
        driver
)

# WICHTIG: Damit LVGL die Konfiguration findet (lv_conf.h)
target_compile_definitions(${COMPONENT_LIB} PUBLIC "-DLV_CONF_INCLUDE_SIMPLE")