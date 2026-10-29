// RX role: continuous reception (no gaps), packet number / RSSI / SNR /
// statistics on screen. Port of the Arduino rx_screen.h.

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lora_radio.h"
#include "roles.h"
#include "ui.h"

// Screen layout (y positions)
#define Y_CONFIG    34
#define Y_PKT_LBL   58
#define Y_PKT       70
#define Y_RSSI_LBL  108
#define Y_RSSI      120
#define Y_SNR_LBL   146
#define Y_SNR       158
#define Y_MSG_LBL   184
#define Y_MSG       196
#define Y_STAT_LBL  222
#define Y_STAT      234
#define Y_LOST      244
#define Y_FERR      254
#define Y_LAST      272

static uint32_t rx_ok, rx_crc, rx_other, rx_lost;
static long rx_last_seq = -1;
static int64_t rx_last_us;

static void draw_static_screen(void)
{
    lcd_fill_screen(UI_BG);
    ui_header("LoRa RX", COLOR_CYAN);
    ui_config(Y_CONFIG, "");
    ui_label(Y_PKT_LBL,  "PACKET");
    ui_label(Y_RSSI_LBL, "RSSI");
    ui_label(Y_SNR_LBL,  "SNR");
    ui_label(Y_MSG_LBL,  "MESSAGE");
    ui_label(Y_STAT_LBL, "STATISTICS");
    ui_text(4, Y_PKT, 4, UI_LABEL, "---", 7);
    ui_text(4, Y_LAST, 2, UI_WARN, "Waiting...", 14);
}

// Packet number = the number after the last '#' in the message, or -1.
static long parse_seq(const char *msg)
{
    const char *p = strrchr(msg, '#');
    if (!p || !isdigit((unsigned char)p[1])) return -1;
    return strtol(p + 1, NULL, 10);
}

static void draw_stats(void)
{
    char buf[40];
    snprintf(buf, sizeof(buf), "OK %lu  CRC %lu  Err %lu",
             (unsigned long)rx_ok, (unsigned long)rx_crc, (unsigned long)rx_other);
    ui_text(4, Y_STAT, 1, UI_TEXT, buf, 27);
    uint32_t total = rx_ok + rx_lost;
    snprintf(buf, sizeof(buf), "Lost %lu  (%.1f%%)",
             (unsigned long)rx_lost, total ? 100.0f * rx_lost / total : 0.0f);
    ui_text(4, Y_LOST, 1, rx_lost ? UI_WARN : UI_TEXT, buf, 27);
}

static void show_packet(const LoraPacket *pkt)
{
    const char *msg = (const char *)pkt->data;
    rx_ok++;
    rx_last_us = esp_timer_get_time();

    // Gaps in the packet numbers count as lost packets.
    // A smaller number means the transmitter restarted: just resync.
    long seq = parse_seq(msg);
    if (seq >= 0 && rx_last_seq >= 0 && seq > rx_last_seq + 1) {
        rx_lost += seq - rx_last_seq - 1;
    }
    if (seq >= 0) rx_last_seq = seq;

    char buf[40];
    if (seq >= 0) snprintf(buf, sizeof(buf), "#%ld", seq);
    else          snprintf(buf, sizeof(buf), "%lu", (unsigned long)rx_ok);
    ui_text(4, Y_PKT, 4, UI_TEXT, buf, 7);

    // RSSI: -110 dBm (weak) .. -30 dBm (strong)
    uint16_t c = pkt->rssi > -80 ? UI_GOOD : (pkt->rssi > -100 ? UI_WARN : UI_BAD);
    snprintf(buf, sizeof(buf), "%.0f dBm", pkt->rssi);
    ui_text(4, Y_RSSI, 2, c, buf, 8);
    ui_bar(104, Y_RSSI + 2, 62, 12, pkt->rssi, -110, -30, c);

    // SNR: LoRa still decodes below 0 dB; -10 .. +15 dB
    c = pkt->snr > 5 ? UI_GOOD : (pkt->snr > -5 ? UI_WARN : UI_BAD);
    snprintf(buf, sizeof(buf), "%.1f dB", pkt->snr);
    ui_text(4, Y_SNR, 2, c, buf, 8);
    ui_bar(104, Y_SNR + 2, 62, 12, pkt->snr, -10, 15, c);

    ui_message(Y_MSG, msg);
    draw_stats();

    snprintf(buf, sizeof(buf), "Freq err %+.0f Hz", pkt->freq_err);
    ui_text(4, Y_FERR, 1, UI_LABEL, buf, 27);
}

static void show_error(int16_t state)
{
    if (state == RADIOLIB_ERR_CRC_MISMATCH) rx_crc++;
    else rx_other++;
    draw_stats();
}

// "Last RX" age and the status line; redraws at most every 250 ms.
static void update_clock(void)
{
    static int64_t last_draw;
    int64_t now = esp_timer_get_time();
    if (now - last_draw < 250000) return;
    last_draw = now;

    if (rx_ok > 0) {
        float age = (now - rx_last_us) / 1e6f;
        char buf[20];
        if (age < 100) snprintf(buf, sizeof(buf), "Last %.1fs", age);
        else           snprintf(buf, sizeof(buf), "Last %.0fs", age);
        ui_text(4, Y_LAST, 2, age < 3 ? UI_GOOD : UI_BAD, buf, 14);
    }
    ui_status_line();
}

void role_rx_run(void)
{
    draw_static_screen();
    printf("LORA1280T RX (ESP-IDF)\n");

    int16_t state = lora_init();
    if (state == RADIOLIB_ERR_NONE) state = lora_start_receive();
    if (state != RADIOLIB_ERR_NONE) {
        printf("SX1280 Fail! Code: %d\n", state);
        char buf[16];
        snprintf(buf, sizeof(buf), "Init %d", state);
        ui_text(4, Y_LAST, 2, UI_BAD, buf, 14);
        for (;;) vTaskDelay(portMAX_DELAY);
    }
    printf("SX1280 Success! Listening...\n");

    static LoraPacket pkt;  // 270 bytes, keep it off the task stack
    for (;;) {
        // Sleeps on the DIO1 semaphore; wakes at least every 250 ms for the clock.
        state = lora_wait_packet(&pkt, 250);
        if (state == RADIOLIB_ERR_NONE) {
            printf("RX Success! Message: %s\n", (const char *)pkt.data);
            printf("[RSSI]: %.2f dBm\n[SNR]: %.2f dB\n", pkt.rssi, pkt.snr);
            show_packet(&pkt);
        } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
            printf("Fail!: CRC Error.\n");
            show_error(state);
        } else if (state != RADIOLIB_ERR_RX_TIMEOUT) {
            printf("Fail, Code: %d\n", state);
            show_error(state);
        }
        update_clock();
    }
}
