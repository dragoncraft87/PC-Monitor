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
   