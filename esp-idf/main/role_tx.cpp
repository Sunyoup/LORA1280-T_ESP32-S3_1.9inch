// TX role: sends "Hello LoRa 2.4G! #n" every CONFIG_LORA_TX_INTERVAL_MS.

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lora_radio.h"
#include "roles.h"
#include "sdkconfig.h"
#include "ui.h"

// Screen layout (y positions), same as the Arduino version
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

static void draw_static_screen(void)
{
    lcd_fill_screen(UI_BG);
    ui_header("LoRa TX", COLOR_ORANGE);
    char extra[16];
    snprintf(extra, sizeof(extra), "%+ddBm", CONFIG_LORA_TX_POWER_DBM);
    ui_config(Y_CONFIG, extra);
    ui_label(Y_PKT_LBL,  "PACKET");
    ui_label(Y_STAT_LBL, "STATUS");
    ui_label(Y_MSG_LBL,  "MESSAGE");
    ui_label(Y_TIME_LBL, "TX TIME / TIME ON AIR");
    ui_label(Y_CNT_LBL,  "SENT / FAILED");
}

void role_tx_run(void)
{
    draw_static_screen();
    printf("LORA1280T TX (ESP-IDF)\n");

    int16_t state = lora_init();
    char buf[48];
    if (state != RADIOLIB_ERR_NONE) {
        printf("SX1280 Fail! Code: %d\n", state);
        snprintf(buf, sizeof(buf), "Init %d", state);
        ui_text(4, Y_STAT, 2, UI_BAD, buf, 14);
        for (;;) vTaskDelay(portMAX_DELAY);
    }
    printf("SX1280 Success!\n");
    ui_text(4, Y_STAT, 2, UI_GOOD, "Ready", 14);

    uint32_t counter = 0, sent_ok = 0, sent_fail = 0;
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        char message[40];
        snprintf(message, sizeof(message), "Hello LoRa 2.4G! #%lu", (unsigned long)counter);
        size_t len = strlen(message);

        snprintf(buf, sizeof(buf), "#%lu", (unsigned long)counter);
        ui_text(4, Y_PKT, 4, UI_TEXT, buf, 7);
        ui_text(4, Y_STAT, 2, UI_WARN, "Sending", 14);
        ui_message(Y_MSG, message);

        // Interrupt-driven: the task sleeps on a semaphore until TX done.
        uint32_t tx_us = 0;
        state = lora_transmit((const uint8_t *)message, len, 1000, &tx_us);

        if (state == RADIOLIB_ERR_NONE) {
            printf("[SX1280] Tranmitting... Success! Message: %s\n", message);
            sent_ok++;
            ui_text(4, Y_STAT, 2, UI_GOOD, "OK", 14);
        } else {
            printf("[SX1280] Tranmitting... Fail, Code: %d\n", state);
            sent_fail++;
            snprintf(buf, sizeof(buf), "Fail %d", state);
            ui_text(4, Y_STAT, 2, UI_BAD, buf, 14);
        }

        snprintf(buf, sizeof(buf), "%.1f/%.1f ms", tx_us / 1000.0f, lora_time_on_air_us(len) / 1000.0f);
        ui_text(4, Y_TIME, 2, UI_TEXT, buf, 14);
        snprintf(buf, sizeof(buf), "%lu / %lu", (unsigned long)sent_ok, (unsigned long)sent_fail);
        ui_text(4, Y_CNT, 2, sent_fail ? UI_WARN : UI_TEXT, buf, 14);
        ui_status_line();

        counter++;
        // Fixed period (start to start), unlike delay() after the work.
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(CONFIG_LORA_TX_INTERVAL_MS));
    }
}
