/**
 * @file gui_settings.c
 * @brief GUI Settings Implementation - Load/Save/Apply
 */

#include "gui_settings.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "GUI-SETTINGS";

/* =============================================================================
 * GLOBAL SETTINGS INSTANCE
 * ========================================================================== */
gui_settings_t gui_settings;

/* =============================================================================
 * FILE PATH
 * ========================================================================== */
#define GUI_CONFIG_PATH "/storage/gui_config.bin"

/* =============================================================================
 * INITIALIZE WITH DEFAULTS
 * ========================================================================== */
void gui_settings_init_defaults(gui_settings_t *settings)
{
    memset(settings, 0, sizeof(gui_settings_t));

    settings->magic = GUI_SETTINGS_MAGIC;
    settings->version = GUI_SETTINGS_VERSION;

    /* Screen backgrounds (all black) */
    for (int i = 0; i < SCREEN_COUNT; i++) {
        settings->bg_color[i] = DEFAULT_BG_COLOR;
    }

    /* Screensaver backgrounds */
    settings->ss_bg_color[SCREEN_CPU] = DEFAULT_SS_BG_CPU;
    settings->ss_bg_color[SCREEN_GPU] = DEFAULT_SS_BG_GPU;
    settings->ss_bg_color[SCREEN_RAM] = DEFAULT_SS_BG_RAM;
    settings->ss_bg_color[SCREEN_NET] = DEFAULT_SS_BG_NET;

    /* Arc colors */
    settings->arc_bg_color = DEFAULT_ARC_BG;
    settings->arc_color_cpu = DEFAULT_ARC_CPU;
    settings->arc_color_gpu = DEFAULT_ARC_GPU;

    /* Bar colors */
    settings->bar_bg_color = DEFAULT_BAR_BG;
    settings->bar_color_ram = DEFAULT_BAR_RAM;
    settings->bar_color_ram_warn = DEFAULT_BAR_RAM_WARN;
    settings->bar_color_ram_crit = DEFAULT_BAR_RAM_CRIT;

    /* Network colors */
    settings->net_color_down = DEFAULT_NET_DOWN;
    settings->net_color_up = DEFAULT_NET_UP;
    settings->net_chart_bg = DEFAULT_NET_CHART_BG;
    settings->net_chart_border = DEFAULT_NET_CHART_BORDER;

    /* Text colors */
    settings->text_title_cpu = DEFAULT_TEXT_TITLE_CPU;
    settings->text_title_gpu = DEFAULT_TEXT_TITLE_GPU;
    settings->text_title_ram = DEFAULT_TEXT_TITLE_RAM;
    settings->text_title_net = DEFAULT_TEXT_TITLE_NET;
    settings->text_value = DEFAULT_TEXT_VALUE;
    settings->text_secondary = DEFAULT_TEXT_SECONDARY;

    /* Temperature colors */
    settings->temp_cold = DEFAULT_TEMP_COLD;
    settings->temp_warm = DEFAULT_TEMP_WARM;
    settings->temp_hot = DEFAULT_TEMP_HOT;

    /* Status colors */
    settings->color_error = DEFAULT_COLOR_ERROR;
    settings->color_ok = DEFAULT_COLOR_OK;

    ESP_LOGI(TAG, "Initialized with default Desert-Spec theme");
}

/* =============================================================================
 * LOAD FROM LITTLEFS
 * ========================================================================== */
bool gui_settings_load(void)
{
    FILE *f = fopen(GUI_CONFIG_PATH, "rb");
    if (f == NULL) {
        ESP_LOGW(TAG, "No gui_config.bin found, using defaults");
        gui_settings_init_defaults(&gui_settings);
        /* Save defaults to LittleFS for next boot */
        gui_settings_save();
        return false;
    }

    gui_settings_t temp;
    size_t read = fread(&temp, 1, sizeof(gui_settings_t), f);
    fclose(f);

    if (read != sizeof(gui_settings_t)) {
        ESP_LOGE(TAG, "Config file size mismatch (read %d, expected %d)",
                 (int)read, (int)sizeof(gui_settings_t));
        gui_settings_init_defaults(&gui_settings);
        return false;
    }

    /* Validate magic number */
    if (temp.magic != GUI_SETTINGS_MAGIC) {
        ESP_LOGE(TAG, "Invalid magic number: 0x%08X (expected 0x%08X)",
                 (unsigned)temp.magic, (unsigned)GUI_SETTINGS_MAGIC);
        gui_settings_init_defaults(&gui_settings);
        return false;
    }

    /* Check version and migrate if needed */
    if (temp.version != GUI_SETTINGS_VERSION) {
        ESP_LOGW(TAG, "Config version mismatch (file: %d, current: %d), migrating...",
                 temp.version, GUI_SETTINGS_VERSION);
        /* For now, just use defaults. Future: add migration logic */
        gui_settings_init_defaults(&gui_settings);
        gui_settings_save();
        return false;
    }

    /* Copy to global instance */
    memcpy(&gui_settings, &temp, sizeof(gui_settings_t));
    ESP_LOGI(TAG, "Loaded GUI settings from LittleFS (version %d)", gui_settings.version);

    return true;
}

/* =============================================================================
 * SAVE TO LITTLEFS
 * ========================================================================== */
bool gui_settings_save(void)
{
    FILE *f = fopen(GUI_CONFIG_PATH, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open gui_config.bin for writing");
        return false;
    }

    /* Ensure magic and version are set */
    gui_settings.magic = GUI_SETTINGS_MAGIC;
    gui_settings.version = GUI_SETTINGS_VERSION;

    size_t written = fwrite(&gui_settings, 1, sizeof(gui_settings_t), f);
    fclose(f);

    if (written != sizeof(gui_settings_t)) {
        ESP_LOGE(TAG, "Failed to write complete config (wrote %d of %d bytes)",
                 (int)written, (int)sizeof(gui_settings_t));
        return false;
    }

    ESP_LOGI(TAG, "Saved GUI settings to LittleFS (%d bytes)", (int)written);
    return true;
}

/* =============================================================================
 * COMMAND HANDLER FOR SET_SS_BG
 *
 * Format: SET_SS_BG=<slot>,<hexcode>
 * Example: SET_SS_BG=0,FF0000 (Sets slot 0 background to red)
 * ========================================================================== */

/* Forward declaration - will be set by ui_manager */
static void (*s_theme_apply_callback)(void) = NULL;

void gui_settings_set_theme_callback(void (*callback)(void))
{
    s_theme_apply_callback = callback;
}

bool gui_settings_handle_command(const char *line)
{
    /* Format: "SET_SS_BG=<slot>,<hex>" */
    if (strncmp(line, "SET_SS_BG=", 10) == 0) {
        int slot;
        unsigned long hex_val;

        /* Parse: Slot (0-3) and Color (Hex e.g. FF0000) */
        if (sscanf(line + 10, "%d,%lx", &slot, &hex_val) == 2) {
            if (slot >= 0 && slot < SCREEN_COUNT) {
                ESP_LOGI(TAG, "Setting SS BG Color for slot %d to 0x%06lX", slot, hex_val);

                /* 1. Set value */
                gui_settings.ss_bg_color[slot] = (uint32_t)hex_val;

                /* 2. Save to LittleFS */
                gui_settings_save();

                /* 3. Trigger theme update via callback (thread-safe) */
                if (s_theme_apply_callback) {
                    s_theme_apply_callback();
                }

                return true;
            } else {
                ESP_LOGW(TAG, "SET_SS_BG: Invalid slot %d (must be 0-3)", slot);
            }
        } else {
            ESP_LOGW(TAG, "SET_SS_BG: Parse error for '%s'", line + 10);
        }
    }
    return false;
}
