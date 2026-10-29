
#define TFT_CS         6
#define TFT_RST        5
#define TFT_DC         7
#define TFT_BL        38
#define TFT_MOSI       9
#define TFT_MISO      -1
#define TFT_SCLK       8
#define GFX_BL    TFT_BL

Arduino_DataBus *bus = new Arduino_ESP32SPI(
    TFT_DC, /* DC */
    TFT_CS, /* CS */
    TFT_SCLK, /* SCLK */
    TFT_MOSI, /* MOSI(SDA) */
    TFT_MISO /* MISO */
);
Arduino_GFX *gfx = new Arduino_ST7789(
    bus,
    TFT_RST,
    0, /* rotation */
    true, /* IPS */
    170, /* width */
    320, /* height */
    35, /* 34 col offset 1 */
    0, /* row offset 1 */
    35, /* 34 col offset 2 */
    0 /* row offset 2 */
);

