#include "lora_radio.h"

#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"

static const char *TAG = "lora";

// SPI3: the display owns SPI2. 8 MHz is well within the SX1280 limit (18 MHz).
static EspHal *s_hal = new EspHal(LORA_PIN_SCK, LORA_PIN_MISO, LORA_PIN_MOSI, SPI3_HOST, 8000000);
static SX1280 s_radio = new Module(s_hal, LORA_PIN_NSS, LORA_PIN_DIO1, LORA_PIN_RST, LORA_PIN_BUSY);

static SemaphoreHandle_t s_dio1;

static void IRAM_ATTR dio1_isr(void)
{
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(s_dio1, &woken);
    if (woken) portYIELD_FROM_ISR();
}

#if CONFIG_LORA_BW_1625
#define LORA_BW_KHZ 1625.0f
#else
#define LORA_BW_KHZ 812.5f
#endif

int16_t lora_init(void)
{
    s_dio1 = xSemaphoreCreateBinary();

    // LORA1280-T datasheet: TCXOEN high before reset, then wait >= 3 ms.
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << LORA_PIN_TCXOEN,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
    gpio_set_level((gpio_num_t)LORA_PIN_TCXOEN, 1);
    vTaskDelay(pdMS_TO_TICKS(5));

    int16_t state = s_radio.begin(CONFIG_LORA_FREQ_KHZ / 1000.0f, LORA_BW_KHZ,
                                  CONFIG_LORA_SF, CONFIG_LORA_CR);
    if (state != RADIOLIB_ERR_NONE) {
        ESP_LOGE(TAG, "begin failed: %d", state);
        return state;
    }
    state = s_radio.setOutputPower(CONFIG_LORA_TX_POWER_DBM);
    if (state != RADIOLIB_ERR_NONE) {
        ESP_LOGE(TAG, "setOutputPower failed: %d", state);
        return state;
    }
    s_radio.setDio1Action(dio1_isr);
    ESP_LOGI(TAG, "SX1280 ready: %.1f MHz, BW %.1f kHz, SF%d, CR4/%d, %d dBm",
             CONFIG_LORA_FREQ_KHZ / 1000.0f, LORA_BW_KHZ, CONFIG_LORA_SF,
             CONFIG_LORA_CR, CONFIG_LORA_TX_POWER_DBM);
    return RADIOLIB_ERR_NONE;
}

int16_t lora_transmit(const uint8_t *data, size_t len, uint32_t timeout_ms, uint32_t *tx_us)
{
    xSemaphoreTake(s_dio1, 0);  // drop a stale event
    int64_t t0 = esp_timer_get_time();
    int16_t state = s_radio.startTransmit(data, len);
    if (state != RADIOLIB_ERR_NONE) return state;

    bool done = xSemaphoreTake(s_dio1, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
    if (tx_us) *tx_us = (uint32_t)(esp_timer_get_time() - t0);
    s_radio.finishTransmit();  // clear IRQ, back to standby
    return done ? RADIOLIB_ERR_NONE : RADIOLIB_ERR_TX_TIMEOUT;
}

int16_t lora_start_receive(void)
{
    xSemaphoreTake(s_dio1, 0);
    return s_radio.startReceive();
}

int16_t lora_wait_packet(LoraPacket *pkt, uint32_t timeout_ms)
{
    if (xSemaphoreTake(s_dio1, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        return RADIOLIB_ERR_RX_TIMEOUT;
    }

    // Read the packet and its status first: readData() puts the radio in
    // standby, and the packet status is lost once RX restarts.
    size_t len = s_radio.getPacketLength();
    if (len > LORA_MAX_PAYLOAD) len = LORA_MAX_PAYLOAD;
    int16_t state = s_radio.readData(pkt->data, len);
    pkt->len = len;
    pkt->data[len] = 0;
    pkt->rssi = s_radio.getRSSI();
    pkt->snr = s_radio.getSNR();
    pkt->freq_err = s_radio.getFrequencyError();

    // Restart RX right away so nothing is missed while the caller prints/draws.
    lora_start_receive();
    return state;
}

uint32_t lora_time_on_air_us(size_t len)
{
    return (uint32_t)s_radio.getTimeOnAir(len);
}

SX1280 &lora_radio(void)
{
    return s_radio;
}
