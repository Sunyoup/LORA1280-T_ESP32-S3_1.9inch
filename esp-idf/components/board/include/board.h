// ESP32-S3 1.9" LCD battery board (SCH_ESP32-S3-LCD-BAT_2026-05-14.pdf)
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Board pins
#define BOARD_PWR_EN    15  // HIGH: LCD power, battery divider (on battery power)
#define BOARD_LCD_BL    38  // backlight (Q1 AO3400), HIGH = on
#define BOARD_KEY       14  // KEY button, 10k pull-up, LOW = pressed
#define BOARD_BAT_ADC   4   // battery divider 100k/100k -> ADC1_CH3

// Turn on LCD power (PWR_EN), set up backlight PWM, KEY and battery ADC.
void board_init(void);

// Backlight brightness 0..100 %.
void board_backlight(uint8_t percent);

// true while the KEY button is held down.
bool board_key_pressed(void);

// Battery (or USB, when plugged in) voltage in mV. 0 if the ADC failed.
// The divider is only connected while PWR_EN is HIGH or USB is present.
uint32_t board_battery_mv(void);

#ifdef __cplusplus
}
#endif
