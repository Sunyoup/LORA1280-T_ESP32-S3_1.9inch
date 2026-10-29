// Small UI helpers for the 170x320 ST7789 (portrait).
// Text is drawn with a background colour and padded to a fixed width,
// so fields can be overwritten in place without clearing (no flicker).
// The same file is used by the TX, RX and RX_Interrupt sketches.
#pragma once

#define UI_BG      RGB565_BLACK
#define UI_LABEL   RGB565(120, 120, 120)
#define UI_TEXT    RGB565_WHITE
#define UI_GOOD    RGB565(0, 220, 0)
#define UI_WARN    RGB565_YELLOW
#define UI_BAD     RGB565_RED
#define UI_BAR_BG  RGB565(40, 40, 40)

// Size-1 font cell is 6x8 px; size N is (6N)x(8N).
#define UI_CHAR_W(size) (6 * (size))
#define UI_CHAR_H(size) (8 * (size))

// Print text at (x, y), padded with spaces to `width` characters.
static void uiText(int16_t x, int16_t y, uint8_t size, uint16_t color,
                   const char *text, uint8_t width)
{
    char buf[48];
    if (width >= sizeof(buf)) width = sizeof(buf) - 1;
    snprintf(buf, sizeof(buf), "%-*.*s", width, width, text);
    gfx->setTextSize(size);
    gfx->setTextColor(color, UI_BG);
    gfx->setCursor(x, y);
    gfx->print(buf);
}

// Small grey caption above a value.
static void uiLabel(int16_t y, const char *text)
{
    uiText(4, y, 1, UI_LABEL, text, 27);
}

// Horizontal bar: `value` mapped from [lo, hi] onto the bar width.
static void uiBar(int16_t x, int16_t y, int16_t w, int16_t h,
                  float value, float lo, float hi, uint16_t color)
{
    float f = (value - lo) / (hi - lo);
    if (f < 0) f = 0;
    if (f > 1) f = 1;
    int16_t fill = (int16_t)(f * w);
    gfx->fillRect(x, y, fill, h, color);
    gfx->fillRect(x + fill, y, w - fill, h, UI_BAR_BG);
}

// Coloured title bar across the top of the screen.
static void uiHeader(const char *title, uint16_t color)
{
    gfx->fillRect(0, 0, gfx->width(), 28, color);
    gfx->setTextSize(2);
    gfx->setTextColor(UI_BG, color);
    gfx->setCursor(6, 6);
    gfx->print(title);
}

// Radio settings, two lines under the header.
static void uiConfig(int16_t y, float freq, float bw, uint8_t sf, uint8_t cr,
                     const char *extra)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f MHz  SF%u", freq, sf);
    uiText(4, y, 1, UI_TEXT, buf, 27);
    snprintf(buf, sizeof(buf), "BW %.1fk CR4/%u %s", bw, cr, extra);
    uiText(4, y + 10, 1, UI_TEXT, buf, 27);
}

// "Up hh:mm:ss" at the bottom of the screen.
static void uiUptime()
{
    uint32_t s = millis() / 1000;
    char buf[20];
    snprintf(buf, sizeof(buf), "Up %02lu:%02lu:%02lu",
             (unsigned long)(s / 3600), (unsigned long)(s / 60 % 60), (unsigned long)(s % 60));
    uiText(4, gfx->height() - 12, 1, UI_LABEL, buf, 27);
}

// Message text, wrapped onto up to two 27-character lines.
static void uiMessage(int16_t y, const String &msg)
{
    uiText(4, y, 1, UI_TEXT, msg.substring(0, 27).c_str(), 27);
    uiText(4, y + 10, 1, UI_TEXT, msg.length() > 27 ? msg.substring(27, 54).c_str() : "", 27);
}
