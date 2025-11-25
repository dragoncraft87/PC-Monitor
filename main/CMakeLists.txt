# ============================================================================
# WÄHLE HIER: TEST MODE oder NORMAL MODE
# ============================================================================
set(USE_TEST_MODE OFF)  # ON = Test Mode (1 Display), OFF = Normal Mode (4 Displays)

if(USE_TEST_MODE)
    # TEST MODE - Nur 1 Display testen
    idf_component_register(
        SRCS
            "main_lvgl_test.c"
            "lvgl_gc9a01_driver.c"
        INCLUDE_DIRS
            "."
        REQUIRES
            lvgl
            esp_lcd
            driver
    )
    message(STATUS "=== BUILDING IN TEST MODE (Single Display) ===")
else()
    # NORMAL MODE - Alle 4 Displays
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
    message(STATUS "=== BUILDING IN NORMAL MODE (4 Displays) ===")
endif()

# WICHTIG: Damit LVGL die Konfiguration findet (lv_conf.h)
# target_compile_definitions(${COMPONENT_LIB} PUBLIC "-DLV_CONF_INCLUDE_SIMPLE")