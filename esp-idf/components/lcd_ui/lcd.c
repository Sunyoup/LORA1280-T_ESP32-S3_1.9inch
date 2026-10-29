#include "lcd.h"

#include <string.h>

#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_st7789.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "font5x7.h"

static const char *TAG = "lcd";

#define LCD_HOST       SPI2_HOST      // the radio uses SPI3
#define LCD_PCLK_HZ    (40 * 1000 * 1000)
#define LCD_X_GAP      35             // 170-wide panel sits at column 35 of the 240-wide controller
#define LCD_Y_GAP      0

// One DMA buffer, big enough for a full-width line of size-4 text (170 x 32).
#define BUF_PIXELS     (LCD_WIDTH * 32)

static esp_lcd_panel_handle_t s_panel;
static uint16_t *s_buf;
static SemaphoreHandle_t s_done;

static bool on_color_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *edata, void *ctx)
{
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(s_done, &woken);
    return woken == pdTRUE;
}

// The panel wants big-endian RGB565 on the wire.
static inline uint16_t swap16(uint16_t c) { return (uint16_t)((c >> 8) | (c << 8)); }

// Send w x h pixels from s_buf and wait until the DMA is done
// (so the buffer can be reused right away).
static void flush(int x, int y, int w, int h)
{
    esp_lcd_panel_draw_bitmap(s_panel, x, y, x + w, y + h, s_buf);
    xSemaphoreTake(s_done, portMAX_DELAY);
}

void lcd_init(void)
{
    s_done = xSemaphoreCreateBinary();
    s_buf = heap_caps_malloc(BUF_PIXELS * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!s_buf) {
        ESP_LOGE(TAG, "no DMA memory for the LCD buffer");
        return;
    }

    spi_bus_config_t bus = {
        .sclk_io_num = LCD_PIN_SCLK,
        .mosi_io_num = LCD_PIN_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = BUF_PIXELS * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = LCD_PIN_DC,
        .cs_gpio_num = LCD_PIN_CS,
        .pclk_hz = LCD_PCLK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 4,
        .on_color_trans_done = on_color_done,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &io));

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io, &panel_cfg, &s_panel));
    esp_lcd_panel_reset(s_panel);
    esp_lcd_panel_init(s_panel);
    esp_lcd_panel_invert_color(s_panel, true);  // IPS panel
    esp_lcd_panel_set_gap(s_panel, LCD_X_GAP, LCD_Y_GAP);
    lcd_fill_screen(COLOR_BLACK);
    esp_lcd_panel_disp_on_off(s_panel, true);
}

void lcd_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    if (!s_buf) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > LCD_WIDTH) w = LCD_WIDTH - x;
    if (y + h > LCD_HEIGHT) h = LCD_HEIGHT - y;
    if (w <= 0 || h <= 0) return;

    uint16_t c = swap16(color);
    int rows_per_chunk = BUF_PIXELS / w;
    int fill = (h < rows_per_chunk ? h : rows_per_chunk) * w;
    for (int i = 0; i < fill; i++) s_buf[i] = c;

    while (h > 0) {
        int rows = h < rows_per_chunk ? h : rows_per_chunk;
        flush(x, y, w, rows);
        y += rows;
        h -= rows;
    }
}

void lcd_fill_screen(uint16_t color)
{
    lcd_fill_rect(0, 0, LCD_WIDTH, LCD_HEIGHT, color);
}

void lcd_text(int x, int y, uint8_t size, uint16_t fg, uint16_t bg, const char *text)
{
    if (!s_buf || size == 0) return;
    int cw = 6 * size, ch = 8 * size;
    int n = (int)strlen(text);
    int max_chars = (LCD_WIDTH - x) / cw;  // clip at the right edge
    if (n > max_chars) n = max_chars;
    if (n <= 0 || y < 0 || y + ch > LCD_HEIGHT) return;

    uint16_t f = swap16(fg), b = swap16(bg);
    int per_chunk = BUF_PIXELS / (cw * ch);  // characters that fit in the buffer

    for (int start = 0; start < n; start += per_chunk) {
        int cnt = n - start < per_chunk ? n - start : per_chunk;
        int w = cnt * cw;
        for (int i = 0; i < cnt; i++) {
            const uint8_t *glyph = &font5x7[(uint8_t)text[start + i] * 5];
            for (int col = 0; col < 6; col++) {
                uint8_t bits = col < 5 ? glyph[col] : 0;  // 6th column is spacing
                for (int row = 0; row < 8; row++) {
                    uint16_t px = (bits >> row) & 1 ? f : b;
                    int bx = i * cw + col * size, by = row * size;
                    for (int dy = 0; dy < size; dy++) {
                        uint16_t *p = &s_buf[(by + dy) * w + bx];
                        for (int dx = 0; dx < size; dx++) p[dx] = px;
                    }
                }
            }
        }
        flush(x + start * cw, y, w, ch);
    }
}
