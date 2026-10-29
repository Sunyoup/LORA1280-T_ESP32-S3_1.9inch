
#include <Arduino_GFX_Library.h>
#include "display_st7789.h"
#include "lora_ui.h"
int32_t w, h;

#include <RadioLib.h>
#include <SPI.h>

#define LORA_MOSI   12
#define LORA_MISO   13
#define LORA_SCLK   11
#define LORA_CS     10

#define LORA_TCXOEN 17
#define LORA_RST    18
#define LORA_BUSY   21
#define LORA_DIO1   16

// Radio settings (shared by radio.begin() and the screen)
#define LORA_FREQ   2440.0  // MHz, keep within 2402~2482 (KR band 2400~2483.5)
#define LORA_BW     812.5
#define LORA_SF     9
#define LORA_CR     7
#define LORA_POWER  7       // dBm (-18 ~ +13); KR limit 10 mW/MHz -> keep <= 10 at BW 812.5

// The display uses FSPI (SPI2), so give the radio its own HSPI (SPI3)
SPIClass loraSPI(HSPI);
SX1280 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY, loraSPI);

int counter = 0;
uint32_t sentOk = 0, sentFail = 0;

#define PWR_EN 15

// Screen layout (y positions)
#define Y_CONFIG    34
#define Y_PKT_LBL   60
#define Y_PKT       72
#define Y_STAT_LBL  112
#define Y_STAT      124
#define Y_MSG_LBL   150
#define Y_MSG       162
#define Y_TIME_LBL  190
#define Y_TIME      202
#define Y_CNT_LBL   232
#define Y_CNT       244

void drawStaticScreen()
{
    gfx->fillScreen(UI_BG);
    gfx->setTextWrap(false);
    uiHeader("LoRa TX", RGB565_ORANGE);
    char extra[16];
    snprintf(extra, sizeof(extra), "%+ddBm", LORA_POWER);
    uiConfig(Y_CONFIG, LORA_FREQ, LORA_BW, LORA_SF, LORA_CR, extra);
    uiLabel(Y_PKT_LBL,  "PACKET");
    uiLabel(Y_STAT_LBL, "STATUS");
    uiLabel(Y_MSG_LBL,  "MESSAGE");
    uiLabel(Y_TIME_LBL, "TX TIME / TIME ON AIR");
    uiLabel(Y_CNT_LBL,  "SENT / FAILED");
}

void setup()
{
    pinMode(PWR_EN, OUTPUT);
    digitalWrite(PWR_EN, 1);

    Serial.begin(115200);
    // USB CDC: do not block when no serial monitor is open
    // (default 100 ms wait per write slows the TX loop to ~1 packet / 3 s)
    Serial.setTxTimeoutMs(0);
    Serial.println("LORA1280T TX!");

    // Init Display
    if (!gfx->begin())
    // if (!gfx->begin(80000000))
    {
      Serial.println("gfx->begin() failed!");
    }

    w = gfx->width();
    h = gfx->height();

    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH);

    drawStaticScreen();

    pinMode(LORA_TCXOEN, OUTPUT);
    digitalWrite(LORA_TCXOEN, HIGH);
    delay(5);

    loraSPI.begin(LORA_SCLK, LORA_MISO, LORA_MOSI, LORA_CS);
    int state = radio.begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR);

    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("SX1280 Success!");
        radio.setOutputPower(LORA_POWER);
        uiText(4, Y_STAT, 2, UI_GOOD, "Ready", 14);
    } else {
        Serial.print("SX1280 Fail! Code: ");
        Serial.println(state);
        char buf[16];
        snprintf(buf, sizeof(buf), "Init %d", state);
        uiText(4, Y_STAT, 2, UI_BAD, buf, 14);
        while (true) { delay(10); }
    }
}

void loop()
{
    Serial.print("[SX1280] Tranmitting... ");
    String message = "Hello LoRa 2.4G! #" + String(counter);

    // Show what is about to be sent
    char buf[32];
    snprintf(buf, sizeof(buf), "#%d", counter);
    uiText(4, Y_PKT, 4, UI_TEXT, buf, 7);
    uiText(4, Y_STAT, 2, UI_WARN, "Sending", 14);
    uiMessage(Y_MSG, message);

    // Transmit message (blocking)
    uint32_t t0 = micros();
    int state = radio.transmit(message);
    uint32_t txUs = micros() - t0;

    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("Success! Message: " + message);
        sentOk++;
        uiText(4, Y_STAT, 2, UI_GOOD, "OK", 14);
    } else if (state == RADIOLIB_ERR_PACKET_TOO_LONG) {
        Serial.println(F("Fail!: Packet too long."));
        sentFail++;
        uiText(4, Y_STAT, 2, UI_BAD, "Too long", 14);
    } else {
        Serial.print("Fail, Code: ");
        Serial.println(state);
        sentFail++;
        snprintf(buf, sizeof(buf), "Fail %d", state);
        uiText(4, Y_STAT, 2, UI_BAD, buf, 14);
    }

    snprintf(buf, sizeof(buf), "%.1f/%.1f ms",
             txUs / 1000.0f, radio.getTimeOnAir(message.length()) / 1000.0f);
    uiText(4, Y_TIME, 2, UI_TEXT, buf, 14);
    snprintf(buf, sizeof(buf), "%lu / %lu", (unsigned long)sentOk, (unsigned long)sentFail);
    uiText(4, Y_CNT, 2, sentFail ? UI_WARN : UI_TEXT, buf, 14);
    uiUptime();

    counter++;
    delay(1000);
}
