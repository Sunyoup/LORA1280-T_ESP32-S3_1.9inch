
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

// The display uses FSPI (SPI2), so give the radio its own HSPI (SPI3)
SPIClass loraSPI(HSPI);
SX1280 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY, loraSPI);

volatile bool receivedFlag = false;

#if defined(ESP8266) || defined(ESP32)
  ICACHE_RAM_ATTR
#endif
void setFlag(void)
{
    receivedFlag = true;
}

#define PWR_EN 15

#include "rx_screen.h"

void setup()
{
    pinMode(PWR_EN, OUTPUT);
    digitalWrite(PWR_EN, 1);

    Serial.begin(115200);
    // USB CDC: do not block when no serial monitor is open
    Serial.setTxTimeoutMs(0);
    Serial.println("LORA1280T!");

    if (!gfx->begin()) {
      Serial.println("gfx->begin() failed!");
    }

    w = gfx->width();
    h = gfx->height();

    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH);

    rxDrawStaticScreen("LoRa RX INT");

    pinMode(LORA_TCXOEN, OUTPUT);
    digitalWrite(LORA_TCXOEN, HIGH);
    delay(5);

    loraSPI.begin(LORA_SCLK, LORA_MISO, LORA_MOSI, LORA_CS);

    int state = radio.begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("SX1280 Success!");
    } else {
        Serial.print("SX1280 Fail! Code: ");
        Serial.println(state);
        rxShowInitError(state);
        while (true) { delay(10); }
    }

    radio.setPacketReceivedAction(setFlag);

    Serial.print("[SX1280] Listening ... ");
    state = radio.startReceive();
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("success!"));
    } else {
        Serial.print("failed, code ");
        Serial.println(state);
        rxShowInitError(state);
        while (true) { delay(10); };
    }
}

void loop()
{
    if (receivedFlag) {
        receivedFlag = false;

        // Read the packet and its status first: readData() puts the radio
        // in standby, and the packet status is lost once RX restarts.
        String message;
        int state = radio.readData(message);
        float rssi = radio.getRSSI();
        float snr = radio.getSNR();
        float ferr = radio.getFrequencyError();

        // Restart RX before printing/drawing, so the radio keeps
        // listening while the slow serial and screen output runs.
        radio.startReceive();

        if (state == RADIOLIB_ERR_NONE) {
            Serial.println("RX Success! Message: " + message);
            Serial.print("[RSSI]: ");
            Serial.print(rssi);
            Serial.println(" dBm");
            Serial.print("[SNR]: ");
            Serial.print(snr);
            Serial.println(" dB");
            rxShowPacket(message, rssi, snr, ferr);
        } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
            Serial.println(F("Fail!: CRC Error."));
            rxShowError(state);
        } else {
            Serial.print("Fail, Code: ");
            Serial.println(state);
            rxShowError(state);
        }
    }

    rxUpdateClock();
}
