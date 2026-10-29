// ST7789 170x320 (portrait) on SPI2 using the ESP-IDF esp_lcd driver,
// with a minimal 6x8-cell text renderer (same font as Arduino_GFX).
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LCD_WIDTH   170
#define LCD_HEIGHT  320

// LCD pins
#define LCD_PIN_SCLK  8
#define LCD_PIN_MOSI  9
#define LCD_PIN_CS    6
#define LCD_PIN_DC    7
#define LCD_PIN_RST   5

#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

#define COLOR_BLACK   RGB565(0, 0, 0)
#define COLOR_WHITE   RGB565(248, 252, 248)
#define COLOR_RED     RGB565(248, 0, 0)
#define COLOR_YELLOW  RGB565(248, 252, 0)
#define COLOR_ORANGE  RGB565(248, 164, 0)
#define COLOR_CYAN    RGB565(0, 252, 248)
#define COLOR_MAGENTA RGB565(248, 0, 248)

// Initialise SPI2 + panel. Call board_init() first (LCD power).
void lcd_init(void);

void lcd_fill_rect(int x, int y, int w, int h, uint16_t color);
void lcd_fill_screen(uint16_t color);

// Draw text with a solid background. Size N = (6N x 8N) pixels per char.
// Text is clipped at the right edge of the screen.
void lcd_text(int x, int y, uint8_t size, uint16_t fg, uint16_t bg, const char *text);

#ifdef __cplusplus
}
#endif
