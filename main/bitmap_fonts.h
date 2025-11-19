#ifndef BITMAP_FONTS_H
#define BITMAP_FONTS_H

#include <stdint.h>

// Font sizes available
#define FONT_WIDTH_8   8
#define FONT_HEIGHT_8  8
#define FONT_WIDTH_16  8
#define FONT_HEIGHT_16 16
#define FONT_WIDTH_24  16
#define FONT_HEIGHT_24 24
#define FONT_WIDTH_32  16
#define FONT_HEIGHT_32 32

// 8x8 Font - Small (for labels like "CPU", "GPU", "RAM")
extern const uint8_t font_8x8[][8];

// 8x16 Font - Medium (for temperatures, details)
extern const uint8_t font_8x16[][16];

// 16x24 Font - Large (for percentages)
extern const uint8_t font_16x24[][48];

// 16x32 Font - XLarge (for main values)
extern const uint8_t font_16x32[][64];

// Helper: Get character index (returns 0 for unsupported chars)
int get_char_index(char c);

#endif // BITMAP_FONTS_H
