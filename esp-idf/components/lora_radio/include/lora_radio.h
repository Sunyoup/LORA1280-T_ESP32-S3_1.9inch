// LORA1280-T (SX1280) on SPI3 via RadioLib's ESP-IDF HAL.
// DIO1 interrupts release a FreeRTOS semaphore, so callers can wait for
// "TX done" / "packet received" with a timeout while the CPU stays free.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <RadioLib.h>

// Radio pins
#define LORA_PIN_SCK     11
#define LORA_PIN_MISO    13
#define LORA_PIN_MOSI    12
#define LORA_PIN_NSS     10
#define LORA_PIN_RST     18
#define LORA_PIN_BUSY    21
#define LORA_PIN_DIO1    16
#define LORA_PIN_TCXOEN  17   // TCXO must be on before reset and stay on

#define LORA_MAX_PAYLOAD 255

struct LoraPacket {
    uint8_t data[LORA_MAX_PAYLOAD + 1];  // +1 so text payloads can be NUL-terminated
    size_t len;
    float rssi;       // dBm
    float snr;        // dB
    float freq_err;   // Hz
};

// TCXO on, SPI3 + RadioLib init with the menuconfig settings. Returns a RadioLib state.
int16_t lora_init(void);

// Send a packet and wait (up to timeout_ms) for TX done. tx_us = measured TX time.
int16_t lora_transmit(const uint8_t *data, size_t len, uint32_t timeout_ms, uint32_t *tx_us);

// Start continuous reception (no gaps between packets).
int16_t lora_start_receive(void);

// Wait up to timeout_ms for a packet. Returns:
//   RADIOLIB_ERR_NONE         packet in *pkt (reception already restarted)
//   RADIOLIB_ERR_RX_TIMEOUT   nothing arrived within timeout_ms (still receiving)
//   other                     error from readData (reception restarted)
int16_t lora_wait_packet(LoraPacket *pkt, uint32_t timeout_ms);

// Expected time on air for a payload of `len` bytes, in microseconds.
uint32_t lora_time_on_air_us(size_t len);

// Direct access for features not wrapped here (e.g. ranging).
SX1280 &lora_radio(void);
