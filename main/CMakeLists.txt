idf_component_register(
    SRCS
        # Main application
        "main_lvgl.c"

        # Drivers
        "lvgl_gc9a01_driver.c"
        "drivers/usb_serial_comm.c"

        # Storage modules
        "storage/storage_mgr.c"
        "storage/hw_identity.c"
        "gui_settings.c"

        # UI modules
        "ui/ui_manager.c"
        "ui/screensaver_mgr.c"

        # Screen implementations
        "screens/screen_cpu_lvgl.c"
        "screens/screen_gpu_lvgl.c"
        "screens/screen_ram_lvgl.c"
        "screens/screen_network_lvgl.c"

        # Compiled images (fallback screensavers)
        "images/DK_Fass.c"
        "images/PacMan.c"
        "images/Sonic_Ring.c"
        "images/triforce.c"

    INCLUDE_DIRS
        "."
        "core"
        "screens"
        "storage"
        "drivers"
        "ui"

    REQUIRES
        lvgl
        esp_lcd
        driver
)

littlefs_create_partition_image(storage lfs FLASH_IN_PROJECT)
