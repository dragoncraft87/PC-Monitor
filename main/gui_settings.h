/**
 * @file gui_settings.h
 * @brief Global GUI Settings Structure for PC Monitor
 *
 * Phase 1.5 - Full GUI Customization
 * All visual parameters are stored here and persisted to LittleFS
 */

#ifndef GUI_SETTINGS_H
#define GUI_SETTINGS_H

#include <stdint.h>
#include <stdbool.h>

/* =============================================================================
 * SCREEN INDICES
 * ========================================================================== */
#define SCREEN_CPU      0
#define SCREEN_GPU      1
#define SCREEN_RAM      2
#define SCREEN_NET      3
#define SCREEN_COUNT    4

/* =============================================================================
 * GUI SETTINGS STRUCTURE
 * ========================================================================== */
typedef struct {
    /* Magic number for validation */
    uint32_t magic;

    /* Version for future migrations */
    uint16_t version;

    /* ========================================================================
     * SCREEN BACKGROUNDS (Normal Mode)
     * ======================================================================== */
    uint32_t bg_color[SCREEN_COUNT];        // Background color per screen

    /* ========================================================================
     * SCREENSAVER SETTINGS
     * ======================================================================== */
    uint32_t ss_bg_color[SCREEN_COUNT];     // Screensaver background per screen

    /* ========================================================================
     * ARC/GAUGE COLORS (CPU & GPU)
     * ======================================================================== */
    uint32_t arc_bg_color;                  // Arc background (gray track)
    uint32_t arc_color_cpu;                 // CPU arc indicator color
    uint32_t arc_color_gpu;                 // GPU arc indicator color

    /* ========================================================================
     * BAR COLORS (RAM)
     * ======================================================================== */
    uint32_t bar_bg_color;                  // Bar background color
    uint32_t bar_color_ram;                 // RAM bar indicator (normal)
    uint32_t bar_color_ram_warn;            // RAM bar (warning >70%)
    uint32_t bar_color_ram_crit;            // RAM bar (critical >85%)

    /* ========================================================================
     * CHART COLORS (Network)
     * ======================================================================== */
    uint32_t net_color_down;                // Download line color (cyan)
    uint32_t net_color_up;                  // Upload line color (magenta)
    uint32_t net_chart_bg;                  // Chart background
    uint32_t net_chart_border;              // Chart border color

    /* ========================================================================
     * TEXT COLORS
     * ======================================================================== */
    uint32_t text_title_cpu;                // CPU title color
    uint32_t text_title_gpu;                // GPU title color
    uint32_t text_title_ram;                // RAM title color
    uint32_t text_title_net;                // Network header color
    uint32_t text_value;                    // Value text (white)
    uint32_t text_secondary;                // Secondary text (gray)

    /* ========================================================================
     * TEMPERATURE COLORS
     * ======================================================================== */
    uint32_t temp_cold;                     // Cold (<60C)
    uint32_t temp_warm;                     // Warm (60-70C)
    uint32_t temp_hot;                      // Hot (>70C)

    /* ========================================================================
     * ERROR/STATUS COLORS
     * ======================================================================== */
    uint32_t color_error;                   // Error/N/A color (red)
    uint32_t color_ok;                      // OK/success color (green)

} gui_settings_t;

/* =============================================================================
 * MAGIC & VERSION
 * ========================================================================== */
#define GUI_SETTINGS_MAGIC      0x47554930  // "GUI0"
#define GUI_SETTINGS_VERSION    1

/* =============================================================================
 * DEFAULT VALUES (Desert-Spec Theme)
 * ========================================================================== */
#define DEFAULT_BG_COLOR            0x000000    // Black

/* Screensaver backgrounds */
#define DEFAULT_SS_BG_CPU           0x00008B    // Dark Blue (Sonic)
#define DEFAULT_SS_BG_GPU           0x8B0000    // Dark Red (Triforce)
#define DEFAULT_SS_BG_RAM           0x5D4037    // Dark Brown (DK)
#define DEFAULT_SS_BG_NET           0x000000    // Black (PacMan)

/* Arc colors */
#define DEFAULT_ARC_BG              0x55555C    // Dark Gray
#define DEFAULT_ARC_CPU             0x0071C5    // Intel Blue
#define DEFAULT_ARC_GPU             0x76B900    // NVIDIA Green

/* Bar colors (RAM) */
#define DEFAULT_BAR_BG              0x222222    // Dark Gray
#define DEFAULT_BAR_RAM             0x43E97B    // Green
#define DEFAULT_BAR_RAM_WARN        0xFFA500    // Orange
#define DEFAULT_BAR_RAM_CRIT        0xFF4444    // Red

/* Network chart colors */
#define DEFAULT_NET_DOWN            0x00FFFF    // Cyan
#define DEFAULT_NET_UP              0xFF00FF    // Magenta
#define DEFAULT_NET_CHART_BG        0x001428    // Dark Teal
#define DEFAULT_NET_CHART_BORDER    0x00FFFF    // Cyan

/* Text colors */
#define DEFAULT_TEXT_TITLE_CPU      0x0071C5    // Intel Blue
#define DEFAULT_TEXT_TITLE_GPU      0x76B900    // NVIDIA Green
#define DEFAULT_TEXT_TITLE_RAM      0x888888    // Gray
#define DEFAULT_TEXT_TITLE_NET      0x00FFFF    // Cyan
#define DEFAULT_TEXT_VALUE          0xFFFFFF    // White
#define DEFAULT_TEXT_SECONDARY      0x888888    // Gray

/* Temperature colors */
#define DEFAULT_TEMP_COLD           0x4CAF50    // Green
#define DEFAULT_TEMP_WARM           0xFF6B6B    // Orange-Red
#define DEFAULT_TEMP_HOT            0xFF4444    // Bright Red

/* Status colors */
#define DEFAULT_COLOR_ERROR         0xFF4444    // Red
#define DEFAULT_COLOR_OK            0x4CAF50    // Green

/* =============================================================================
 * GLOBAL SETTINGS INSTANCE (extern)
 * ========================================================================== */
extern gui_settings_t gui_settings;

/* =============================================================================
 * FUNCTION PROTOTYPES
 * ========================================================================== */

/**
 * @brief Initialize GUI settings with defaults
 */
void gui_settings_init_defaults(gui_settings_t *settings);

/**
 * @brief Load GUI settings from LittleFS
 * @return true if loaded successfully, false if defaults were used
 */
bool gui_settings_load(void);

/**
 * @brief Save GUI settings to LittleFS
 * @return true if saved successfully
 */
bool gui_settings_save(void);

/**
 * @brief Apply current theme to all UI elements (live update)
 * Must be called from LVGL context (with mutex held)
 */
void gui_apply_theme(void);

#endif /* GUI_SETTINGS_H */
