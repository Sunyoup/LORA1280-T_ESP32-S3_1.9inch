#include "ui.h"

#include <stdio.h>
#include <string.h>

#include "board.h"
#include "esp_timer.h"
#include "sdkconfig.h"

void ui_text(int x, int y, uint8_t size, uint16_t color, const char *text, uint8_t width)
{
    char buf[48];
    if (width >= sizeof(buf)) width = sizeof(buf) - 1;
    snprintf(buf, sizeof(buf), "%-*.*s", width, width, text);
    lcd_text(x, y, size, color, UI_BG, buf);
}

void ui_label(int y, const char *text)
{
    ui_text(4, y, 1, UI_LABEL, text, 27);
}

void ui_bar(int x, int y, int w, int h, float value, float lo, float hi, uint16_t color)
{
    float f = (value - lo) / (hi - lo);
    if (f < 0) f = 0;
    if (f > 1) f = 1;
    int fill = (int)(f * w);
    lcd_fill_rect(x, y, fill, h, color);
    lcd_fill_rect(x + fill, y, w - fill, h, UI_BAR_BG);
}

void ui_header(const char *title, uint16_t color)
{
    lcd_fill_rect(0, 0, LCD_WIDTH, 28, color);
    lcd_text(6, 6, 2, UI_BG, color, title);
}

void ui_config(int y, const char *extra)
{
    char buf[40];
    snprintf(buf, sizeof(buf), "%.1f MHz  SF%d", CONFIG_LORA_FREQ_KHZ / 1000.0, CONFIG_LORA_SF);
    ui_text(4, y, 1, UI_TEXT, buf, 27);
#if CONFIG_LORA_BW_1625
    const char *bw = "1625.0";
#else
    const char *bw = "812.5";
#endif
    snprintf(buf, sizeof(buf), "BW %sk CR4/%d %s", bw, CONFIG_LORA_CR, extra);
    ui_text(4, y + 10, 1, UI_TEXT, buf, 27);
}

void ui_status_line(void)
{
    uint32_t s = (uint32_t)(esp_timer_get_time() / 1000000);
    uint32_t mv = board_battery_mv();
    char bat[12];
    if (mv == 0) {
        snprintf(bat, sizeof(bat), "Bat ?");
    } else if (mv > 4400) {
        snprintf(bat, sizeof(bat), "USB");  // divider sees the USB rail
    } else {
        snprintf(bat, sizeof(bat), "Bat %lu.%02luV", (unsigned long)(mv / 1000), (unsigned long)(mv % 1000 / 10));
    }
    char buf[40];
    snprintf(buf, sizeof(buf), "Up %02lu:%02lu:%02lu  %s",
             (unsigned long)(s / 3600), (unsigned long)(s / 60 % 60), (unsigned long)(s % 60), bat);
    ui_text(4, LCD_HEIGHT - 12, 1, UI_LABEL, buf, 27);
}

void ui_message(int y, const char *msg)
{
    char line[28];
    size_t len = strlen(msg);
    snprintf(line, sizeof(line), "%.27s", msg);
    ui_text(4, y, 1, UI_TEXT, line, 27);
    snprintf(line, sizeof(line), "%.27s", len > 27 ? msg + 27 : "");
    ui_text(4, y + 10, 1, UI_TEXT, line, 27);
}
