// Small UI helpers (port of the Arduino lora_ui.h).
// Text is drawn with a background colour and padded to a fixed width,
// so fields can be overwritten in place without clearing (no flicker).
#pragma once

#include <stdint.h>

#include "lcd.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UI_BG      COLOR_BLACK
#define UI_LABEL   RGB565(120, 120, 120)
#define UI_TEXT    COLOR_WHITE
#define UI_GOOD    RGB565(0, 220, 0)
#define UI_WARN    COLOR_YELLOW
#define UI_BAD     COLOR_RED
#define UI_BAR_BG  RGB565(40, 40, 40)

// Text at (x, y), padded with spaces to `width` characters.
void ui_text(int x, int y, uint8_t size, uint16_t color, const char *text, uint8_t width);

// Small grey caption above a value.
void ui_label(int y, const char *text);

// Horizontal bar: `value` mapped from [lo, hi] onto the bar width.
void ui_bar(int x, int y, int w, int h, float value, float lo, float hi, uint16_t color);

// Coloured title bar across the top of the screen.
void ui_header(const char *title, uint16_t color);

// Radio settings, two lines under the header. `extra` is appended to line 2.
void ui_config(int y, const char *extra);

// Bottom line: "Up hh:mm:ss  Bat 3.92V" (or "USB" when powered by USB).
void ui_status_line(void);

// Message text, wrapped onto up to two 27-character lines.
void ui_message(int y, const char *msg);

#ifdef __cplusplus
}
#endif
