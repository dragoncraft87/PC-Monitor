/**
 * @file main_lvgl.c
 * @brief PC Monitor - Desert-Spec Phase 2 Edition
 *
 * Key Features:
 * - Robust line-buffer parser (no partial line processing)
 * - 100ms display update (10 FPS, Watchdog-friendly)
 * - Red dot indicator when data is stale (>1s)
 * - Screensaver after 5s no data (retro game themes)
 * - Crash-resistant design throughout
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/usb_serial_jtag.h"

#include "lvgl.h"
#include "lvgl_gc9a01_driver.h"
#include "screens/screens_lvgl.h"
#include "esp_littlefs.h"
#include "gui_settings.h"
#include "screensaver_images.h"

// Include GUI settings implementation
#include "gui_settings.c"

// Include screensaver images implementation
#include "screensaver_images.c"

// Include screen implementations
#include "screens/screen_cpu_lvgl.c"
#include "screens/screen_gpu_lvgl.c"
#include "screens/screen_ram_lvgl.c"
#include "screens/screen_network_lvgl.c"

// Include screensaver images
#include "images/Sonic_Ring.c"
#include "images/triforce.c"
#include "images/DK_Fass.c"
#include "images/PacMan.c"

static const char *TAG = "PC-MONITOR";

/* =============================================================================
 * CONFIGURATION - Desert-Spec Phase 2
 * ========================================================================== */
#define SCREENSAVER_TIMEOUT_MS   30000  // 30 seconds no data -> screensaver
#define STALE_DATA_THRESHOLD_MS  2000   // 2 seconds -> show red dot
#define DISPLAY_UPDATE_MS        100    // 10 FPS - Watchdog friendly
#define LINE_BUFFER_SIZE         2048   // Max line length (larger for IMG_DATA chunks)

/* =============================================================================
 * SCREENSAVER THEME COLORS (now from gui_settings, macros for legacy compat)
 * ========================================================================== */
#define COLOR_SONIC_BG      lv_color_hex(gui_settings.ss_bg_color[SCREEN_CPU])
#define COLOR_ART_BG        lv_color_hex(gui_settings.ss_bg_color[SCREEN_GPU])
#define COLOR_DK_BG         lv_color_hex(gui_settings.ss_bg_color[SCREEN_RAM])
#define COLOR_PACMAN_BG     lv_color_hex(gui_settings.ss_bg_color[SCREEN_NET])

/* =============================================================================
 * GLOBAL STATE
 * ========================================================================== */
static pc_stats_t pc_stats;
static SemaphoreHandle_t stats_mutex;
static SemaphoreHandle_t lvgl_mutex;
static volatile uint32_t last_data_ms = 0;

/* Display Handles */
static lvgl_gc9a01_handle_t display_cpu, display_gpu, display_ram, display_network;

/* Screen Handles */
static screen_cpu_t *screen_cpu = NULL;
static screen_gpu_t *screen_gpu = NULL;
static screen_ram_t *screen_ram = NULL;
static screen_network_t *screen_network = NULL;

/* Red Dot Indicators (stale data warning) */
static lv_obj_t *dot_cpu = NULL;
static lv_obj_t *dot_gpu = NULL;
static lv_obj_t *dot_ram = NULL;
static lv_obj_t *dot_net = NULL;

/* Screensaver Objects */
static lv_obj_t *screensaver_cpu = NULL;
static lv_obj_t *screensaver_gpu = NULL;
static lv_obj_t *screensaver_ram = NULL;
static lv_obj_t *screensaver_net = NULL;
static bool is_screensaver_active = false;

/* SPI Pin Configurations */
static const lvgl_gc9a01_config_t config_cpu = {
    .pin_sck = 4, .pin_mosi = 5, .pin_cs = 12,
    .pin_dc = 11, .pin_rst = 13, .spi_host = SPI2_HOST
};
static const lvgl_gc9a01_config_t config_gpu = {
    .pin_sck = 4, .pin_mosi = 5, .pin_cs = 9,
    .pin_dc = 46, .pin_rst = 10, .spi_host = SPI2_HOST
};
static const lvgl_gc9a01_config_t config_ram = {
    .pin_sck = 4, .pin_mosi = 5, .pin_cs = 8,
    .pin_dc = 18, .pin_rst = 3, .spi_host = SPI2_HOST
};
static const lvgl_gc9a01_config_t config_net = {
    .pin_sck = 4, .pin_mosi = 5, .pin_cs = 16,
    .pin_dc = 15, .pin_rst = 17, .spi_host = SPI2_HOST
};

/* =============================================================================
 * DYNAMIC HARDWARE NAMES (loaded from LittleFS)
 * ========================================================================== */
#define STORAGE_MOUNT_POINT "/storage"
#define NAMES_FILE_PATH     STORAGE_MOUNT_POINT "/names.txt"
#define HASH_FILE_PATH      STORAGE_MOUNT_POINT "/host.hash"

typedef struct {
    char cpu_name[32];
    char gpu_name[32];
    char identity_hash[16];  // 8 hex chars + null
} hw_names_t;

static hw_names_t hw_names = {
    .cpu_name = "CPU",
    .gpu_name = "GPU",
    .identity_hash = "00000000"
};

/* =============================================================================
 * LITTLEFS: Mount with Auto-Format (Desert-Spec: must always start)
 * ========================================================================== */
static esp_err_t storage_init(void)
{
    ESP_LOGI(TAG, "Initializing LittleFS storage...");

    esp_vfs_littlefs_conf_t conf = {
        .base_path = STORAGE_MOUNT_POINT,
        .partition_label = "storage",
        .format_if_mount_failed = true,  // Desert-Spec: auto-format on error
        .dont_mount = false,
    };

    esp_err_t ret = esp_vfs_littlefs_register(&conf);

    if (ret == ESP_OK) {
        size_t total = 0, used = 0;
        esp_littlefs_info(conf.partition_label, &total, &used);
        ESP_LOGI(TAG, "LittleFS mounted: %d KB total, %d KB used",
                 (int)(total / 1024), (int)(used / 1024));
    } else if (ret == ESP_ERR_NOT_FOUND) {
        ESP_LOGE(TAG, "LittleFS partition 'storage' not found!");
    } else {
        ESP_LOGE(TAG, "LittleFS mount failed: %s", esp_err_to_name(ret));
    }

    return ret;
}

/* =============================================================================
 * HARDWARE NAMES: Load from /storage/names.txt and /storage/host.hash
 * Format: CPU_NAME=i9-7980XE\nGPU_NAME=3080 Ti\n
 * ========================================================================== */
static void load_hardware_names(void)
{
    // Load names
    FILE *f = fopen(NAMES_FILE_PATH, "r");
    if (f != NULL) {
        char line[64];
        while (fgets(line, sizeof(line), f) != NULL) {
            // Remove newline
            char *nl = strchr(line, '\n');
            if (nl) *nl = '\0';
            char *cr = strchr(line, '\r');
            if (cr) *cr = '\0';

            // Parse KEY=VALUE
            if (strncmp(line, "CPU_NAME=", 9) == 0) {
                strncpy(hw_names.cpu_name, line + 9, sizeof(hw_names.cpu_name) - 1);
                hw_names.cpu_name[sizeof(hw_names.cpu_name) - 1] = '\0';
                ESP_LOGI(TAG, "Loaded CPU name: %s", hw_names.cpu_name);
            }
            else if (strncmp(line, "GPU_NAME=", 9) == 0) {
                strncpy(hw_names.gpu_name, line + 9, sizeof(hw_names.gpu_name) - 1);
                hw_names.gpu_name[sizeof(hw_names.gpu_name) - 1] = '\0';
                ESP_LOGI(TAG, "Loaded GPU name: %s", hw_names.gpu_name);
            }
        }
        fclose(f);
    } else {
        ESP_LOGW(TAG, "No names.txt found, using defaults");
    }

    // Load identity hash
    FILE *hf = fopen(HASH_FILE_PATH, "r");
    if (hf != NULL) {
        char hash_buf[16];
        if (fgets(hash_buf, sizeof(hash_buf), hf) != NULL) {
            // Remove newline
            char *nl = strchr(hash_buf, '\n');
            if (nl) *nl = '\0';
            if (strlen(hash_buf) == 8) {
                strncpy(hw_names.identity_hash, hash_buf, sizeof(hw_names.identity_hash) - 1);
                ESP_LOGI(TAG, "Loaded identity hash: %s", hw_names.identity_hash);
            }
        }
        fclose(hf);
    } else {
        ESP_LOGW(TAG, "No host.hash found, using default 00000000");
    }
}

/* =============================================================================
 * HARDWARE NAMES: Save to LittleFS
 * ========================================================================== */
static void save_hardware_names(void)
{
    // Save names
    FILE *f = fopen(NAMES_FILE_PATH, "w");
    if (f != NULL) {
        fprintf(f, "CPU_NAME=%s\n", hw_names.cpu_name);
        fprintf(f, "GPU_NAME=%s\n", hw_names.gpu_name);
        fclose(f);
        ESP_LOGI(TAG, "Saved names to LittleFS");
    } else {
        ESP_LOGE(TAG, "Failed to save names.txt");
    }

    // Save hash
    FILE *hf = fopen(HASH_FILE_PATH, "w");
    if (hf != NULL) {
        fprintf(hf, "%s\n", hw_names.identity_hash);
        fclose(hf);
        ESP_LOGI(TAG, "Saved hash to LittleFS: %s", hw_names.identity_hash);
    } else {
        ESP_LOGE(TAG, "Failed to save host.hash");
    }
}

/* =============================================================================
 * UI UPDATE: Apply hardware names to screen labels (call from LVGL context!)
 * ========================================================================== */
static void apply_hardware_names_to_ui(void)
{
    if (screen_cpu && screen_cpu->label_title) {
        lv_label_set_text(screen_cpu->label_title, hw_names.cpu_name);
    }
    if (screen_gpu && screen_gpu->label_title) {
        lv_label_set_text(screen_gpu->label_title, hw_names.gpu_name);
    }
}

/* =============================================================================
 * GUI THEME: Apply current theme to all UI elements (call from LVGL context!)
 * ========================================================================== */
void gui_apply_theme(void)
{
    ESP_LOGI(TAG, "Applying theme...");

    /* --- CPU Screen --- */
    if (screen_cpu) {
        if (screen_cpu->screen) {
            lv_obj_set_style_bg_color(screen_cpu->screen,
                lv_color_hex(gui_settings.bg_color[SCREEN_CPU]), 0);
        }
        if (screen_cpu->arc) {
            lv_obj_set_style_arc_color(screen_cpu->arc,
                lv_color_hex(gui_settings.arc_bg_color), LV_PART_MAIN);
            lv_obj_set_style_arc_color(screen_cpu->arc,
                lv_color_hex(gui_settings.arc_color_cpu), LV_PART_INDICATOR);
        }
        if (screen_cpu->label_title) {
            lv_obj_set_style_text_color(screen_cpu->label_title,
                lv_color_hex(gui_settings.text_title_cpu), LV_PART_MAIN);
        }
    }

    /* --- GPU Screen --- */
    if (screen_gpu) {
        if (screen_gpu->screen) {
            lv_obj_set_style_bg_color(screen_gpu->screen,
                lv_color_hex(gui_settings.bg_color[SCREEN_GPU]), 0);
        }
        if (screen_gpu->arc) {
            lv_obj_set_style_arc_color(screen_gpu->arc,
                lv_color_hex(gui_settings.arc_bg_color), LV_PART_MAIN);
            lv_obj_set_style_arc_color(screen_gpu->arc,
                lv_color_hex(gui_settings.arc_color_gpu), LV_PART_INDICATOR);
        }
        if (screen_gpu->label_title) {
            lv_obj_set_style_text_color(screen_gpu->label_title,
                lv_color_hex(gui_settings.text_title_gpu), LV_PART_MAIN);
        }
    }

    /* --- RAM Screen --- */
    if (screen_ram) {
        if (screen_ram->screen) {
            lv_obj_set_style_bg_color(screen_ram->screen,
                lv_color_hex(gui_settings.bg_color[SCREEN_RAM]), 0);
        }
        if (screen_ram->bar) {
            lv_obj_set_style_bg_color(screen_ram->bar,
                lv_color_hex(gui_settings.bar_bg_color), LV_PART_MAIN);
            // Note: indicator color changes dynamically based on usage
        }
    }

    /* --- Network Screen --- */
    if (screen_network) {
        if (screen_network->screen) {
            lv_obj_set_style_bg_color(screen_network->screen,
                lv_color_hex(gui_settings.bg_color[SCREEN_NET]), 0);
        }
        if (screen_network->chart) {
            lv_obj_set_style_bg_color(screen_network->chart,
                lv_color_hex(gui_settings.net_chart_bg), 0);
            lv_obj_set_style_border_color(screen_network->chart,
                lv_color_hex(gui_settings.net_chart_border), 0);
        }
        if (screen_network->label_header) {
            lv_obj_set_style_text_color(screen_network->label_header,
                lv_color_hex(gui_settings.text_title_net), 0);
        }
    }

    /* --- Screensaver backgrounds --- */
    if (screensaver_cpu) {
        lv_obj_set_style_bg_color(screensaver_cpu,
            lv_color_hex(gui_settings.ss_bg_color[SCREEN_CPU]), 0);
    }
    if (screensaver_gpu) {
        lv_obj_set_style_bg_color(screensaver_gpu,
            lv_color_hex(gui_settings.ss_bg_color[SCREEN_GPU]), 0);
    }
    if (screensaver_ram) {
        lv_obj_set_style_bg_color(screensaver_ram,
            lv_color_hex(gui_settings.ss_bg_color[SCREEN_RAM]), 0);
    }
    if (screensaver_net) {
        lv_obj_set_style_bg_color(screensaver_net,
            lv_color_hex(gui_settings.ss_bg_color[SCREEN_NET]), 0);
    }

    ESP_LOGI(TAG, "Theme applied");
}

/* =============================================================================
 * HELPER: Create Red Dot Indicator
 * ========================================================================== */
static lv_obj_t *create_status_dot(lv_obj_t *parent)
{
    if (!parent) return NULL;

    lv_obj_t *dot = lv_obj_create(parent);
    lv_obj_set_size(dot, 12, 12);  // Radius 6 = Diameter 12
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dot, 1, 0);
    lv_obj_set_style_border_color(dot, lv_color_white(), 0);
    lv_obj_align(dot, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);  // Hidden by default

    return dot;
}

/* =============================================================================
 * HELPER: Create Screensaver Overlay
 * ========================================================================== */
static lv_obj_t *create_screensaver(lv_obj_t *parent, lv_color_t bg_color, const lv_img_dsc_t *icon_src)
{
    if (!parent) return NULL;

    // Fullscreen overlay container
    lv_obj_t *overlay = lv_obj_create(parent);
    lv_obj_set_size(overlay, 240, 240);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_style_bg_color(overlay, bg_color, 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_radius(overlay, 0, 0);
    lv_obj_set_style_pad_all(overlay, 0, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

    // Centered image
    lv_obj_t *img = lv_img_create(overlay);
    lv_img_set_src(img, icon_src);
    lv_obj_center(img);

    // Start hidden
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);

    return overlay;
}

/* =============================================================================
 * HELPER: Show/Hide Screensavers
 * ========================================================================== */
static void show_screensavers(void)
{
    if (screensaver_cpu) lv_obj_clear_flag(screensaver_cpu, LV_OBJ_FLAG_HIDDEN);
    if (screensaver_gpu) lv_obj_clear_flag(screensaver_gpu, LV_OBJ_FLAG_HIDDEN);
    if (screensaver_ram) lv_obj_clear_flag(screensaver_ram, LV_OBJ_FLAG_HIDDEN);
    if (screensaver_net) lv_obj_clear_flag(screensaver_net, LV_OBJ_FLAG_HIDDEN);
}

static void hide_screensavers(void)
{
    if (screensaver_cpu) lv_obj_add_flag(screensaver_cpu, LV_OBJ_FLAG_HIDDEN);
    if (screensaver_gpu) lv_obj_add_flag(screensaver_gpu, LV_OBJ_FLAG_HIDDEN);
    if (screensaver_ram) lv_obj_add_flag(screensaver_ram, LV_OBJ_FLAG_HIDDEN);
    if (screensaver_net) lv_obj_add_flag(screensaver_net, LV_OBJ_FLAG_HIDDEN);
}

/* =============================================================================
 * HELPER: Show/Hide Red Dots
 * ========================================================================== */
static void show_red_dots(void)
{
    if (dot_cpu) lv_obj_clear_flag(dot_cpu, LV_OBJ_FLAG_HIDDEN);
    if (dot_gpu) lv_obj_clear_flag(dot_gpu, LV_OBJ_FLAG_HIDDEN);
    if (dot_ram) lv_obj_clear_flag(dot_ram, LV_OBJ_FLAG_HIDDEN);
    if (dot_net) lv_obj_clear_flag(dot_net, LV_OBJ_FLAG_HIDDEN);
}

static void hide_red_dots(void)
{
    if (dot_cpu) lv_obj_add_flag(dot_cpu, LV_OBJ_FLAG_HIDDEN);
    if (dot_gpu) lv_obj_add_flag(dot_gpu, LV_OBJ_FLAG_HIDDEN);
    if (dot_ram) lv_obj_add_flag(dot_ram, LV_OBJ_FLAG_HIDDEN);
    if (dot_net) lv_obj_add_flag(dot_net, LV_OBJ_FLAG_HIDDEN);
}

/* =============================================================================
 * DATA PARSER - Robust Line Buffer Implementation
 * ========================================================================== */
static void parse_pc_data(const char *line)
{
    if (!line || strlen(line) < 5) return;

    // Make a mutable copy for strtok
    char buffer[LINE_BUFFER_SIZE];
    strncpy(buffer, line, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    // Parse into temporary struct first to avoid partial updates
    pc_stats_t temp_stats = {0};
    int fields_parsed = 0;
    char *token = strtok(buffer, ",");

    while (token != NULL) {
        // CPU Usage (-1 = error)
        if (strncmp(token, "CPU:", 4) == 0) {
            temp_stats.cpu_percent = (int16_t)atoi(token + 4);
            fields_parsed++;
        }
        // CPU Temperature (-1 = error)
        else if (strncmp(token, "CPUT:", 5) == 0) {
            temp_stats.cpu_temp = atof(token + 5);
            fields_parsed++;
        }
        // GPU Usage (-1 = error)
        else if (strncmp(token, "GPU:", 4) == 0) {
            temp_stats.gpu_percent = (int16_t)atoi(token + 4);
            fields_parsed++;
        }
        // GPU Temperature (-1 = error)
        else if (strncmp(token, "GPUT:", 5) == 0) {
            temp_stats.gpu_temp = atof(token + 5);
            fields_parsed++;
        }
        // VRAM
        else if (strncmp(token, "VRAM:", 5) == 0) {
            sscanf(token + 5, "%f/%f", &temp_stats.gpu_vram_used, &temp_stats.gpu_vram_total);
            fields_parsed++;
        }
        // RAM
        else if (strncmp(token, "RAM:", 4) == 0) {
            sscanf(token + 4, "%f/%f", &temp_stats.ram_used_gb, &temp_stats.ram_total_gb);
            if (temp_stats.ram_total_gb < 0.1f) {
                temp_stats.ram_total_gb = 16.0f;
            }
            fields_parsed++;
        }
        // Network Type
        else if (strncmp(token, "NET:", 4) == 0) {
            strncpy(temp_stats.net_type, token + 4, sizeof(temp_stats.net_type) - 1);
            temp_stats.net_type[sizeof(temp_stats.net_type) - 1] = '\0';
            fields_parsed++;
        }
        // Network Speed
        else if (strncmp(token, "SPEED:", 6) == 0) {
            strncpy(temp_stats.net_speed, token + 6, sizeof(temp_stats.net_speed) - 1);
            temp_stats.net_speed[sizeof(temp_stats.net_speed) - 1] = '\0';
            fields_parsed++;
        }
        // Download Speed
        else if (strncmp(token, "DOWN:", 5) == 0) {
            temp_stats.net_down_mbps = atof(token + 5);
            fields_parsed++;
        }
        // Upload Speed
        else if (strncmp(token, "UP:", 3) == 0) {
            temp_stats.net_up_mbps = atof(token + 3);
            fields_parsed++;
        }

        token = strtok(NULL, ",");
    }

    // Only commit to global state if we got enough fields (avoid partial/corrupt updates)
    if (fields_parsed >= 5) {
        xSemaphoreTake(stats_mutex, portMAX_DELAY);
        pc_stats = temp_stats;
        last_data_ms = (uint32_t)(esp_timer_get_time() / 1000);
        xSemaphoreGive(stats_mutex);
        ESP_LOGD(TAG, "Parsed %d fields, timestamp updated", fields_parsed);
    } else {
        ESP_LOGW(TAG, "Incomplete data: only %d fields parsed, discarding", fields_parsed);
    }
}

/* =============================================================================
 * COMMAND HANDLER: Process NAME_CPU, NAME_GPU, NAME_HASH commands
 * Returns true if command was handled, false if it's data
 * ========================================================================== */
static bool handle_name_command(const char *line)
{
    bool needs_save = false;
    bool needs_ui_update = false;

    // NAME_CPU=<cpu_name>
    if (strncmp(line, "NAME_CPU=", 9) == 0) {
        strncpy(hw_names.cpu_name, line + 9, sizeof(hw_names.cpu_name) - 1);
        hw_names.cpu_name[sizeof(hw_names.cpu_name) - 1] = '\0';
        ESP_LOGI(TAG, "Received CPU name: %s", hw_names.cpu_name);
        needs_save = true;
        needs_ui_update = true;
    }
    // NAME_GPU=<gpu_name>
    else if (strncmp(line, "NAME_GPU=", 9) == 0) {
        strncpy(hw_names.gpu_name, line + 9, sizeof(hw_names.gpu_name) - 1);
        hw_names.gpu_name[sizeof(hw_names.gpu_name) - 1] = '\0';
        ESP_LOGI(TAG, "Received GPU name: %s", hw_names.gpu_name);
        needs_save = true;
        needs_ui_update = true;
    }
    // NAME_HASH=<8-char hex>
    else if (strncmp(line, "NAME_HASH=", 10) == 0) {
        if (strlen(line + 10) >= 8) {
            strncpy(hw_names.identity_hash, line + 10, 8);
            hw_names.identity_hash[8] = '\0';
            ESP_LOGI(TAG, "Received identity hash: %s", hw_names.identity_hash);
            needs_save = true;
        }
    }
    else {
        return false;  // Not a NAME command
    }

    // Save to LittleFS if needed
    if (needs_save) {
        save_hardware_names();
    }

    // Update UI labels if needed (must be done in LVGL context)
    if (needs_ui_update) {
        if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            apply_hardware_names_to_ui();
            xSemaphoreGive(lvgl_mutex);
        }
    }

    return true;
}

/* =============================================================================
 * COMMAND HANDLER: Process SET_CLR_* color commands
 * Returns true if command was handled, false if not a SET_CLR command
 * ========================================================================== */
static uint32_t parse_hex_color(const char *hex_str)
{
    // Skip optional 0x prefix
    if (hex_str[0] == '0' && (hex_str[1] == 'x' || hex_str[1] == 'X')) {
        hex_str += 2;
    }
    return (uint32_t)strtoul(hex_str, NULL, 16);
}

static bool handle_color_command(const char *line)
{
    bool needs_save = false;
    bool needs_theme_update = false;

    // SET_CLR_ARC_CPU:RRGGBB
    if (strncmp(line, "SET_CLR_ARC_CPU:", 16) == 0) {
        gui_settings.arc_color_cpu = parse_hex_color(line + 16);
        ESP_LOGI(TAG, "Set CPU arc color: 0x%06X", (unsigned)gui_settings.arc_color_cpu);
        needs_save = needs_theme_update = true;
    }
    // SET_CLR_ARC_GPU:RRGGBB
    else if (strncmp(line, "SET_CLR_ARC_GPU:", 16) == 0) {
        gui_settings.arc_color_gpu = parse_hex_color(line + 16);
        ESP_LOGI(TAG, "Set GPU arc color: 0x%06X", (unsigned)gui_settings.arc_color_gpu);
        needs_save = needs_theme_update = true;
    }
    // SET_CLR_ARC_BG:RRGGBB
    else if (strncmp(line, "SET_CLR_ARC_BG:", 15) == 0) {
        gui_settings.arc_bg_color = parse_hex_color(line + 15);
        ESP_LOGI(TAG, "Set arc bg color: 0x%06X", (unsigned)gui_settings.arc_bg_color);
        needs_save = needs_theme_update = true;
    }
    // SET_CLR_BAR_RAM:RRGGBB
    else if (strncmp(line, "SET_CLR_BAR_RAM:", 16) == 0) {
        gui_settings.bar_color_ram = parse_hex_color(line + 16);
        ESP_LOGI(TAG, "Set RAM bar color: 0x%06X", (unsigned)gui_settings.bar_color_ram);
        needs_save = needs_theme_update = true;
    }
    // SET_CLR_NET_DN:RRGGBB
    else if (strncmp(line, "SET_CLR_NET_DN:", 15) == 0) {
        gui_settings.net_color_down = parse_hex_color(line + 15);
        ESP_LOGI(TAG, "Set net download color: 0x%06X", (unsigned)gui_settings.net_color_down);
        needs_save = needs_theme_update = true;
    }
    // SET_CLR_NET_UP:RRGGBB
    else if (strncmp(line, "SET_CLR_NET_UP:", 15) == 0) {
        gui_settings.net_color_up = parse_hex_color(line + 15);
        ESP_LOGI(TAG, "Set net upload color: 0x%06X", (unsigned)gui_settings.net_color_up);
        needs_save = needs_theme_update = true;
    }
    // SET_CLR_TXT_TITLE:Index:RRGGBB (0=CPU, 1=GPU, 2=RAM, 3=NET)
    else if (strncmp(line, "SET_CLR_TXT_TITLE:", 18) == 0) {
        int idx = line[18] - '0';
        if (idx >= 0 && idx < 4 && line[19] == ':') {
            uint32_t color = parse_hex_color(line + 20);
            switch (idx) {
                case 0: gui_settings.text_title_cpu = color; break;
                case 1: gui_settings.text_title_gpu = color; break;
                case 2: gui_settings.text_title_ram = color; break;
                case 3: gui_settings.text_title_net = color; break;
            }
            ESP_LOGI(TAG, "Set title color[%d]: 0x%06X", idx, (unsigned)color);
            needs_save = needs_theme_update = true;
        }
    }
    // SET_CLR_TXT_VAL:RRGGBB
    else if (strncmp(line, "SET_CLR_TXT_VAL:", 16) == 0) {
        gui_settings.text_value = parse_hex_color(line + 16);
        ESP_LOGI(TAG, "Set value text color: 0x%06X", (unsigned)gui_settings.text_value);
        needs_save = needs_theme_update = true;
    }
    // SET_CLR_BG_NORM:Index:RRGGBB
    else if (strncmp(line, "SET_CLR_BG_NORM:", 16) == 0) {
        int idx = line[16] - '0';
        if (idx >= 0 && idx < 4 && line[17] == ':') {
            gui_settings.bg_color[idx] = parse_hex_color(line + 18);
            ESP_LOGI(TAG, "Set bg color[%d]: 0x%06X", idx, (unsigned)gui_settings.bg_color[idx]);
            needs_save = needs_theme_update = true;
        }
    }
    // SET_CLR_BG_SS:Index:RRGGBB
    else if (strncmp(line, "SET_CLR_BG_SS:", 14) == 0) {
        int idx = line[14] - '0';
        if (idx >= 0 && idx < 4 && line[15] == ':') {
            gui_settings.ss_bg_color[idx] = parse_hex_color(line + 16);
            ESP_LOGI(TAG, "Set screensaver bg[%d]: 0x%06X", idx, (unsigned)gui_settings.ss_bg_color[idx]);
            needs_save = needs_theme_update = true;
        }
    }
    // SET_CLR_TEMP:Type:RRGGBB (0=cold, 1=warm, 2=hot)
    else if (strncmp(line, "SET_CLR_TEMP:", 13) == 0) {
        int idx = line[13] - '0';
        if (idx >= 0 && idx < 3 && line[14] == ':') {
            uint32_t color = parse_hex_color(line + 15);
            switch (idx) {
                case 0: gui_settings.temp_cold = color; break;
                case 1: gui_settings.temp_warm = color; break;
                case 2: gui_settings.temp_hot = color; break;
            }
            ESP_LOGI(TAG, "Set temp color[%d]: 0x%06X", idx, (unsigned)color);
            needs_save = true;  // No immediate UI update needed, used dynamically
        }
    }
    // RESET_THEME - Reset to Desert-Spec defaults
    else if (strcmp(line, "RESET_THEME") == 0) {
        gui_settings_init_defaults(&gui_settings);
        ESP_LOGI(TAG, "Reset to default Desert-Spec theme");
        needs_save = needs_theme_update = true;
    }
    else {
        return false;  // Not a color command
    }

    // Save to LittleFS if needed
    if (needs_save) {
        gui_settings_save();
    }

    // Apply theme if needed (must be done in LVGL context)
    if (needs_theme_update) {
        if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            gui_apply_theme();
            xSemaphoreGive(lvgl_mutex);
        }
    }

    return true;
}

/* =============================================================================
 * TASK: USB Serial RX - Robust Line Buffer
 * ========================================================================== */
static void usb_rx_task(void *arg)
{
    static uint8_t rx_buf[256];
    static char line_buf[LINE_BUFFER_SIZE];
    static int line_pos = 0;
    static bool is_discarding = false;

    ESP_LOGI(TAG, "USB RX Task started");

    while (1) {
        // Read with timeout so other tasks can run
        int len = usb_serial_jtag_read_bytes(rx_buf, sizeof(rx_buf), pdMS_TO_TICKS(10));

        if (len > 0) {
            for (int i = 0; i < len; i++) {
                uint8_t c = rx_buf[i];

                // End of line - process it or end discard mode
                if (c == '\n' || c == '\r') {
                    if (is_discarding) {
                        // End of the oversized line, reset and resume normal operation
                        is_discarding = false;
                        line_pos = 0;
                    } else if (line_pos > 0) {
                        line_buf[line_pos] = '\0';

                        // Handshake: respond to WHO_ARE_YOU? with identity + hash
                        if (strcmp(line_buf, "WHO_ARE_YOU?") == 0) {
                            char response[64];
                            snprintf(response, sizeof(response), "SCARAB_CLIENT_OK|H:%s\n", hw_names.identity_hash);
                            usb_serial_jtag_write_bytes((const uint8_t *)response, strlen(response), pdMS_TO_TICKS(100));
                            ESP_LOGI(TAG, "Handshake: WHO_ARE_YOU? -> %s", response);
                        }
                        // NAME commands from PC client
                        else if (!handle_name_command(line_buf)) {
                            // Try color commands
                            if (!handle_color_command(line_buf)) {
                                // Try image commands
                                if (!handle_image_command(line_buf)) {
                                    // Not a command, try to parse as PC data
                                    parse_pc_data(line_buf);
                                }
                            }
                        }

                        line_pos = 0;  // Reset for next line
                    }
                }
                // In discard mode - ignore everything until newline
                else if (is_discarding) {
                    // Do nothing, wait for newline
                }
                // Normal character - add to buffer
                else {
                    if (line_pos < LINE_BUFFER_SIZE - 1) {
                        line_buf[line_pos++] = (char)c;
                    } else {
                        // Buffer full - discard rest of this line
                        ESP_LOGW(TAG, "Line buffer overflow, discarding rest of line");
                        is_discarding = true;
                    }
                }
            }

            // Yield to watchdog after processing data
            vTaskDelay(pdMS_TO_TICKS(1));
        } else {
            // No data - longer delay
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

/* =============================================================================
 * TASK: Display Update - 10 FPS with Screensaver Logic
 * ========================================================================== */
static void display_update_task(void *arg)
{
    // Initialize timestamp
    last_data_ms = (uint32_t)(esp_timer_get_time() / 1000);

    ESP_LOGI(TAG, "Display Update Task started (10 FPS)");

    while (1) {
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        uint32_t time_since_data = now - last_data_ms;

        bool data_is_stale = (time_since_data > STALE_DATA_THRESHOLD_MS);
        bool should_screensave = (time_since_data > SCREENSAVER_TIMEOUT_MS);

        // Try to take LVGL mutex (don't block forever)
        if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {

            // =================================================================
            // SCREENSAVER LOGIC
            // =================================================================
            if (should_screensave && !is_screensaver_active) {
                is_screensaver_active = true;
                show_screensavers();
                ESP_LOGW(TAG, "Screensaver ON (no data for %lu ms)", time_since_data);
            }
            else if (!should_screensave && is_screensaver_active) {
                is_screensaver_active = false;
                hide_screensavers();
                ESP_LOGI(TAG, "Screensaver OFF (data received)");
            }

            // =================================================================
            // RED DOT LOGIC
            // =================================================================
            if (data_is_stale && !is_screensaver_active) {
                show_red_dots();
            } else {
                hide_red_dots();
            }

            // =================================================================
            // UPDATE SCREENS (only if not in screensaver)
            // =================================================================
            if (!is_screensaver_active) {
                // Get local copy of stats
                xSemaphoreTake(stats_mutex, portMAX_DELAY);
                pc_stats_t local_stats = pc_stats;
                xSemaphoreGive(stats_mutex);

                // Update all screens
                if (screen_cpu) screen_cpu_update(screen_cpu, &local_stats);
                if (screen_gpu) screen_gpu_update(screen_gpu, &local_stats);
                if (screen_ram) screen_ram_update(screen_ram, &local_stats);
                if (screen_network) screen_network_update(screen_network, &local_stats);
            }

            xSemaphoreGive(lvgl_mutex);
        }

        // DESERT-SPEC: 100ms delay = 10 FPS, Watchdog friendly
        vTaskDelay(pdMS_TO_TICKS(DISPLAY_UPDATE_MS));
    }
}

/* =============================================================================
 * TASK: LVGL Tick (10ms)
 * ========================================================================== */
static void lvgl_tick_task(void *arg)
{
    while (1) {
        lv_tick_inc(10);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* =============================================================================
 * TASK: LVGL Timer Handler - Watchdog Friendly
 * ========================================================================== */
static void lvgl_timer_task(void *arg)
{
    ESP_LOGI(TAG, "LVGL Timer Task started");

    while (1) {
        if (xSemaphoreTake(lvgl_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            uint32_t time_till_next = lv_timer_handler();
            xSemaphoreGive(lvgl_mutex);

            // Use the time LVGL tells us to wait, but cap it
            uint32_t delay_ms = (time_till_next < 5) ? 5 : time_till_next;
            if (delay_ms > 30) delay_ms = 30;
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        } else {
            // Couldn't get mutex, yield to other tasks
            vTaskDelay(pdMS_TO_TICKS(5));
        }

        // Extra yield to let IDLE task run (feed watchdog)
        taskYIELD();
    }
}

/* =============================================================================
 * MAIN APPLICATION ENTRY
 * ========================================================================== */
void app_main(void)
{
    ESP_LOGI(TAG, "===========================================");
    ESP_LOGI(TAG, "PC Monitor - Desert-Spec v1.6");
    ESP_LOGI(TAG, "===========================================");

    // Initialize LittleFS storage (Desert-Spec: auto-format on error)
    if (storage_init() == ESP_OK) {
        load_hardware_names();
        gui_settings_load();  // Load GUI theme settings
    } else {
        // Use defaults if storage failed
        gui_settings_init_defaults(&gui_settings);
    }

    // Create mutexes
    stats_mutex = xSemaphoreCreateMutex();
    lvgl_mutex = xSemaphoreCreateMutex();

    if (!stats_mutex || !lvgl_mutex) {
        ESP_LOGE(TAG, "Failed to create mutexes!");
        return;
    }

    // Initialize USB Serial JTAG
    usb_serial_jtag_driver_config_t usb_cfg = {
        .rx_buffer_size = 2048,
        .tx_buffer_size = 1024
    };
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_cfg));
    ESP_LOGI(TAG, "USB Serial JTAG initialized");

    // Initialize SPI Bus
    spi_bus_config_t buscfg = {
        .mosi_io_num = 5,
        .sclk_io_num = 4,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 240 * 240 * 2 + 8
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    ESP_LOGI(TAG, "SPI Bus initialized");

    // Initialize LVGL
    lv_init();
    ESP_LOGI(TAG, "LVGL initialized");

    // Initialize displays and screens (under mutex)
    xSemaphoreTake(lvgl_mutex, portMAX_DELAY);

    // Initialize screensaver image system (load custom images from LFS)
    ss_images_init();

    // Display 1: CPU
    ESP_LOGI(TAG, "Initializing CPU display...");
    lvgl_gc9a01_init(&config_cpu, &display_cpu);
    screen_cpu = screen_cpu_create(lvgl_gc9a01_get_display(&display_cpu));
    if (screen_cpu && screen_cpu->screen) {
        // Apply dynamic hardware name
        if (screen_cpu->label_title) {
            lv_label_set_text(screen_cpu->label_title, hw_names.cpu_name);
        }
        dot_cpu = create_status_dot(screen_cpu->screen);
        screensaver_cpu = create_screensaver(screen_cpu->screen, COLOR_SONIC_BG, ss_image_get_dsc(SS_IMG_CPU));
    }

    // Display 2: GPU
    ESP_LOGI(TAG, "Initializing GPU display...");
    lvgl_gc9a01_init(&config_gpu, &display_gpu);
    screen_gpu = screen_gpu_create(lvgl_gc9a01_get_display(&display_gpu));
    if (screen_gpu && screen_gpu->screen) {
        // Apply dynamic hardware name
        if (screen_gpu->label_title) {
            lv_label_set_text(screen_gpu->label_title, hw_names.gpu_name);
        }
        dot_gpu = create_status_dot(screen_gpu->screen);
        screensaver_gpu = create_screensaver(screen_gpu->screen, COLOR_ART_BG, ss_image_get_dsc(SS_IMG_GPU));
    }

    // Display 3: RAM
    ESP_LOGI(TAG, "Initializing RAM display...");
    lvgl_gc9a01_init(&config_ram, &display_ram);
    screen_ram = screen_ram_create(lvgl_gc9a01_get_display(&display_ram));
    if (screen_ram && screen_ram->screen) {
        dot_ram = create_status_dot(screen_ram->screen);
        screensaver_ram = create_screensaver(screen_ram->screen, COLOR_DK_BG, ss_image_get_dsc(SS_IMG_RAM));
    }

    // Display 4: Network
    ESP_LOGI(TAG, "Initializing Network display...");
    lvgl_gc9a01_init(&config_net, &display_network);
    screen_network = screen_network_create(lvgl_gc9a01_get_display(&display_network));
    if (screen_network && screen_network->screen) {
        dot_net = create_status_dot(screen_network->screen);
        screensaver_net = create_screensaver(screen_network->screen, COLOR_PACMAN_BG, ss_image_get_dsc(SS_IMG_NET));
    }

    xSemaphoreGive(lvgl_mutex);
    ESP_LOGI(TAG, "All displays initialized");

    // Create tasks with WATCHDOG-FRIENDLY priorities
    // All tasks at low priority so IDLE can run and feed watchdog

    // USB RX: Priority 3
    xTaskCreate(usb_rx_task, "usb_rx", 4096, NULL, 3, NULL);

    // LVGL Tick: Priority 2 (just needs to run periodically)
    xTaskCreate(lvgl_tick_task, "lv_tick", 2048, NULL, 2, NULL);

    // LVGL Timer: Priority 2, pinned to Core 1
    // Lower priority = IDLE1 can run and feed watchdog
    xTaskCreatePinnedToCore(lvgl_timer_task, "lv_timer", 8192, NULL, 2, NULL, 1);

    // Display Update: Priority 2, pinned to Core 0
    xTaskCreatePinnedToCore(display_update_task, "update", 4096, NULL, 2, NULL, 0);

    ESP_LOGI(TAG, "===========================================");
    ESP_LOGI(TAG, "System ready. Waiting for USB data...");
    ESP_LOGI(TAG, "Screensaver in %d seconds if no data", SCREENSAVER_TIMEOUT_MS / 1000);
    ESP_LOGI(TAG, "===========================================");
}
