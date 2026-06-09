#include "power_mgmt.h"
#include "moskill_config.h"
#include "ble_service.h"
#include "stats_engine.h"
#include "ui_driver.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "power";

extern adc_oneshot_unit_handle_t shared_adc_handle;
static bool s_hv_enabled = false;
static int64_t s_hv_start_us = 0;
static battery_status_t s_battery = {0};
static int64_t s_last_activity_us = 0;
static uint8_t s_last_notified_pct = 0xFF;

static void adc_bat_init(void)
{
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN,
        .bitwidth = ADC_WIDTH,
    };
    adc_oneshot_config_channel(shared_adc_handle, ADC_CHANNEL_1, &chan_cfg);
}

static uint16_t read_battery_mv(void)
{
    int raw = 0;
    adc_oneshot_read(shared_adc_handle, ADC_CHANNEL_1, &raw);
    float voltage = (float)raw / ADC_MAX_VALUE * 3.3f * BAT_VOLTAGE_DIVIDER_RATIO;
    return (uint16_t)(voltage * 1000);
}

static const uint16_t bat_curve[][2] = {
    {4200, 100}, {4100, 90}, {4000, 80}, {3900, 70},
    {3800, 60}, {3750, 50}, {3700, 40}, {3600, 25},
    {3500, 15}, {3400, 8}, {3300, 4}, {3000, 0},
};

static uint8_t mv_to_percentage(uint16_t mv)
{
    const int n = sizeof(bat_curve) / sizeof(bat_curve[0]);
    if (mv >= bat_curve[0][0]) return (uint8_t)bat_curve[0][1];
    if (mv <= bat_curve[n - 1][0]) return (uint8_t)bat_curve[n - 1][1];

    for (int i = 0; i < n - 1; i++) {
        if (mv <= bat_curve[i][0] && mv >= bat_curve[i + 1][0]) {
            uint16_t v_hi = bat_curve[i][0], v_lo = bat_curve[i + 1][0];
            uint16_t p_hi = bat_curve[i][1], p_lo = bat_curve[i + 1][1];
            return (uint8_t)(p_lo + (uint32_t)(mv - v_lo) * (p_hi - p_lo) / (v_hi - v_lo));
        }
    }
    return 0;
}

void power_mgmt_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_HV_ENABLE),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 1,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(PIN_HV_ENABLE, 0);

    // charge status input
    io_conf.pin_bit_mask = (1ULL << PIN_CHARGE_STATUS);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    io_conf.pull_down_en = 0;
    gpio_config(&io_conf);

    adc_bat_init();

    s_last_activity_us = esp_timer_get_time();
    ESP_LOGI(TAG, "Power management initialized");
}

void power_hv_enable(bool enable)
{
    if (enable && s_battery.charging) {
        ESP_LOGW(TAG, "HV enable refused: battery is charging");
        return;
    }

    s_hv_enabled = enable;
    gpio_set_level(PIN_HV_ENABLE, enable ? 1 : 0);
    s_last_activity_us = esp_timer_get_time();

    if (enable) {
        s_hv_start_us = esp_timer_get_time();
    } else {
        s_hv_start_us = 0;
    }

    ESP_LOGI(TAG, "HV %s", enable ? "ARMED" : "DISARMED");
}

bool power_hv_is_enabled(void)
{
    return s_hv_enabled;
}

battery_status_t power_get_battery(void)
{
    return s_battery;
}

void power_mgmt_task(void *pvParameters)
{
    while (1) {
        s_battery.voltage_mv = read_battery_mv();
        s_battery.percentage = mv_to_percentage(s_battery.voltage_mv);
        s_battery.charging = (gpio_get_level(PIN_CHARGE_STATUS) == 0);
        s_battery.low = (s_battery.percentage <= BAT_LOW_THRESHOLD);

        // notify BLE on 5% change
        if (s_last_notified_pct == 0xFF ||
            abs((int)s_battery.percentage - (int)s_last_notified_pct) >= 5) {
            ble_notify_battery(s_battery.percentage);
            s_last_notified_pct = s_battery.percentage;
        }

        if (s_battery.low && !s_battery.charging) {
            ui_notify_low_battery();
        }

        // HV max duration timeout (10 min)
        if (s_hv_enabled && s_hv_start_us > 0) {
            int64_t hv_on_us = esp_timer_get_time() - s_hv_start_us;
            if (hv_on_us > (600LL * 1000000LL)) {
                ESP_LOGW(TAG, "HV auto-disabled: max duration (600s) exceeded");
                power_hv_enable(false);
            }
        }

        // auto sleep on inactivity
        int64_t idle_us = esp_timer_get_time() - s_last_activity_us;
        if (!s_hv_enabled && idle_us > (DEEP_SLEEP_TIMEOUT_MS * 1000LL)) {
            ESP_LOGI(TAG, "Entering deep sleep (inactivity timeout)");
            // save stats before sleep
            stats_reset_session();
            esp_sleep_enable_gpio_wakeup();
            gpio_wakeup_enable(PIN_BUTTON, GPIO_INTR_LOW_LEVEL);
            esp_deep_sleep_start();
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
