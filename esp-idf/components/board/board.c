#include "board.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "board";

// Backlight PWM. LEDC channel 0 / timer 0 may be used by RadioLib's tone(),
// which this project never calls, but stay clear of it anyway.
#define BL_LEDC_TIMER    LEDC_TIMER_1
#define BL_LEDC_CHANNEL  LEDC_CHANNEL_1
#define BL_LEDC_RES      LEDC_TIMER_10_BIT

static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t s_cali;

void board_init(void)
{
    // PWR_EN first: on battery power the LCD has no supply until this is HIGH.
    gpio_config_t out = {
        .pin_bit_mask = 1ULL << BOARD_PWR_EN,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&out);
    gpio_set_level(BOARD_PWR_EN, 1);
    vTaskDelay(pdMS_TO_TICKS(10));

    gpio_config_t key = {
        .pin_bit_mask = 1ULL << BOARD_KEY,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,  // board has 10k too
    };
    gpio_config(&key);

    ledc_timer_config_t tcfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = BL_LEDC_RES,
        .timer_num = BL_LEDC_TIMER,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&tcfg);
    ledc_channel_config_t ccfg = {
        .gpio_num = BOARD_LCD_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = BL_LEDC_CHANNEL,
        .timer_sel = BL_LEDC_TIMER,
        .duty = 0,  // off until the screen is drawn
    };
    ledc_channel_config(&ccfg);

    adc_oneshot_unit_init_cfg_t ucfg = { .unit_id = ADC_UNIT_1 };
    if (adc_oneshot_new_unit(&ucfg, &s_adc) == ESP_OK) {
        adc_oneshot_chan_cfg_t chcfg = {
            .atten = ADC_ATTEN_DB_12,  // up to ~3.1 V; divided battery is <= 2.4 V
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        adc_oneshot_config_channel(s_adc, ADC_CHANNEL_3, &chcfg);  // GPIO4
        adc_cali_curve_fitting_config_t cal = {
            .unit_id = ADC_UNIT_1,
            .chan = ADC_CHANNEL_3,
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        if (adc_cali_create_scheme_curve_fitting(&cal, &s_cali) != ESP_OK) {
            ESP_LOGW(TAG, "ADC calibration not available, battery reading is raw");
            s_cali = NULL;
        }
    } else {
        ESP_LOGE(TAG, "ADC init failed");
        s_adc = NULL;
    }
}

void board_backlight(uint8_t percent)
{
    if (percent > 100) percent = 100;
    uint32_t duty = ((1u << BL_LEDC_RES) - 1) * percent / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BL_LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BL_LEDC_CHANNEL);
}

bool board_key_pressed(void)
{
    return gpio_get_level(BOARD_KEY) == 0;
}

uint32_t board_battery_mv(void)
{
    if (!s_adc) return 0;
    int sum = 0, n = 0;
    for (int i = 0; i < 8; i++) {
        int mv = 0, raw = 0;
        if (adc_oneshot_read(s_adc, ADC_CHANNEL_3, &raw) != ESP_OK) continue;
        if (s_cali) {
            adc_cali_raw_to_voltage(s_cali, raw, &mv);
        } else {
            mv = raw * 3100 / 4095;  // rough
        }
        sum += mv;
        n++;
    }
    if (n == 0) return 0;
    return (uint32_t)(sum / n) * 2;  // 100k/100k divider
}
