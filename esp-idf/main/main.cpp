// LoRa SX1280 (LORA1280-T) on ESP32-S3 1.9" LCD board, ESP-IDF port.
//
// One firmware for every board. The role (TX / RX, later ranging master /
// slave) is chosen on the device and saved in NVS:
//   - first boot, or KEY held while the board starts -> role menu
//   - role menu: short press = next role, hold 1 s = select and save

#include <stdio.h>

#include "board.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lcd.h"
#include "lora_radio.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "roles.h"
#include "sdkconfig.h"
#include "ui.h"

static const char *TAG = "main";

#define NVS_NS        "lora"
#define NVS_KEY_ROLE  "role"
#define LONG_PRESS_MS 1000

const char *role_name(Role r)
{
    switch (r) {
    case ROLE_TX: return "TX";
    case ROLE_RX: return "RX";
    default:      return "?";
    }
}

static bool load_role(Role *r)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return false;
    uint8_t v = 0;
    esp_err_t err = nvs_get_u8(h, NVS_KEY_ROLE, &v);
    nvs_close(h);
    if (err != ESP_OK || v >= ROLE_COUNT) return false;
    *r = (Role)v;
    return true;
}

static void save_role(Role r)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, NVS_KEY_ROLE, (uint8_t)r);
    nvs_commit(h);
    nvs_close(h);
}

static void draw_menu(Role sel)
{
    for (int i = 0; i < ROLE_COUNT; i++) {
        bool on = (i == sel);
        char buf[16];
        snprintf(buf, sizeof(buf), " %s ", role_name((Role)i));
        lcd_text(20, 70 + i * 40, 3, on ? UI_BG : UI_TEXT, on ? COLOR_MAGENTA : UI_BG, buf);
    }
}

// Role menu driven by the single KEY button.
static Role choose_role(Role current)
{
    lcd_fill_screen(UI_BG);
    ui_header("Select role", COLOR_MAGENTA);
    ui_label(250, "Short press : next");
    ui_label(262, "Hold 1 s    : select & save");
    Role sel = current;
    draw_menu(sel);

    while (board_key_pressed()) vTaskDelay(pdMS_TO_TICKS(10));  // let go of the boot press

    for (;;) {
        while (!board_key_pressed()) vTaskDelay(pdMS_TO_TICKS(10));
        vTaskDelay(pdMS_TO_TICKS(30));  // debounce
        if (!board_key_pressed()) continue;

        TickType_t t0 = xTaskGetTickCount();
        bool long_press = false;
        while (board_key_pressed()) {
            if (!long_press && xTaskGetTickCount() - t0 >= pdMS_TO_TICKS(LONG_PRESS_MS)) {
                long_press = true;
                ui_text(4, 290, 2, UI_GOOD, "Saved", 14);
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        if (long_press) {
            save_role(sel);
            vTaskDelay(pdMS_TO_TICKS(300));
            return sel;
        }
        sel = (Role)((sel + 1) % ROLE_COUNT);
        draw_menu(sel);
    }
}

extern "C" void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    board_init();
    lcd_init();
    board_backlight(100);

    // Sample KEY right away: holding it during reset opens the role menu.
    bool key_at_boot = board_key_pressed();

    Role role;
    bool saved = load_role(&role);
    if (!saved) {
#if CONFIG_APP_DEFAULT_ROLE_RX
        role = ROLE_RX;
#else
        role = ROLE_TX;
#endif
    }
    if (key_at_boot || !saved) {
        role = choose_role(role);
    }
    ESP_LOGI(TAG, "role: %s (hold KEY during reset to change)", role_name(role));

    switch (role) {
    case ROLE_TX: role_tx_run(); break;
    case ROLE_RX: role_rx_run(); break;
    default: break;
    }
}
