#include "kill_detector.h"
#include "moskill_config.h"
#include "moskill_types.h"
#include "env_sensor.h"
#include "power_mgmt.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_timer.h"

static const char *TAG = "kill_det";

static QueueHandle_t s_event_queue;
adc_oneshot_unit_handle_t shared_adc_handle;
static uint16_t s_threshold = KILL_THRESHOLD_LOW;

static detect_state_t s_state = DETECT_STATE_IDLE;
static uint16_t s_peak_adc = 0;
static uint64_t s_energy_accum = 0;
static int64_t s_event_start_us = 0;
static int64_t s_confirm_start_us = 0;
static int64_t s_cooldown_until_us = 0;

static void adc_init(void)
{
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &shared_adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN,
        .bitwidth = ADC_WIDTH,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(shared_adc_handle, ADC_CHANNEL_0, &chan_cfg));
}

static uint16_t adc_read(void)
{
    int raw = 0;
    adc_oneshot_read(shared_adc_handle, ADC_CHANNEL_0, &raw);
    return (uint16_t)raw;
}

static kill_class_t classify_kill(uint16_t peak)
{
    if (peak < KILL_CLASS_S_MAX) return KILL_CLASS_S;
    if (peak < KILL_CLASS_M_MAX) return KILL_CLASS_M;
    if (peak < KILL_CLASS_L_MAX) return KILL_CLASS_L;
    return KILL_CLASS_XL;
}

static void emit_kill_event(uint16_t peak, uint16_t duration_ms, uint32_t energy)
{
    env_reading_t env = env_sensor_get_latest();

    kill_event_t evt = {
        .timestamp = (uint32_t)(esp_timer_get_time() / 1000000),
        .kill_class = classify_kill(peak),
        .peak_adc = peak,
        .duration_ms = duration_ms,
        .energy_proxy = energy,
        .temperature = env.valid ? (int8_t)env.temperature : -128,
        .humidity = env.valid ? (uint8_t)env.humidity : 0,
    };

    if (xQueueSend(s_event_queue, &evt, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Kill event queue full, dropping event");
    }

    ESP_LOGI(TAG, "KILL! class=%d peak=%u dur=%ums energy=%lu",
             evt.kill_class, evt.peak_adc, evt.duration_ms, evt.energy_proxy);
}

void kill_detector_init(QueueHandle_t event_queue)
{
    s_event_queue = event_queue;
    adc_init();
    ESP_LOGI(TAG, "Kill detector initialized, threshold=%u", s_threshold);
}

void kill_detector_set_sensitivity(uint8_t level)
{
    switch (level) {
        case 0: s_threshold = SENSITIVITY_LOW; break;
        case 1: s_threshold = SENSITIVITY_MED; break;
        case 2: s_threshold = SENSITIVITY_HIGH; break;
        default: s_threshold = SENSITIVITY_MED; break;
    }
    ESP_LOGI(TAG, "Sensitivity set to %u (threshold=%u)", level, s_threshold);
}

void kill_detector_task(void *pvParameters)
{
    const TickType_t sample_period = pdMS_TO_TICKS(1000 / ADC_SAMPLE_RATE_HZ);
    int64_t now_us;

    while (1) {
        uint16_t sample = adc_read();
        now_us = esp_timer_get_time();

        switch (s_state) {
        case DETECT_STATE_IDLE:
            if (!power_hv_is_enabled()) break;
            if (now_us < s_cooldown_until_us) break;
            if (sample >= s_threshold) {
                s_state = DETECT_STATE_TRIGGERED;
                s_event_start_us = now_us;
                s_peak_adc = sample;
                s_energy_accum = (uint64_t)sample * sample;
            }
            break;

        case DETECT_STATE_TRIGGERED: {
            uint16_t elapsed_ms = (uint16_t)((now_us - s_event_start_us) / 1000);
            if (sample > s_peak_adc) s_peak_adc = sample;
            s_energy_accum += (uint64_t)sample * sample;

            if (sample < s_threshold && elapsed_ms >= KILL_MIN_SUSTAIN_MS) {
                s_state = DETECT_STATE_CONFIRMING;
                s_confirm_start_us = now_us;
            } else if (elapsed_ms > KILL_MAX_DURATION_MS) {
                ESP_LOGW(TAG, "Continuous short detected, aborting (dur=%ums)", elapsed_ms);
                s_state = DETECT_STATE_IDLE;
                s_cooldown_until_us = now_us + (KILL_COOLDOWN_MS * 1000LL);
            } else if (sample >= KILL_THRESHOLD_HIGH) {
                // clipping — still accumulate but flag
            }
            break;
        }

        case DETECT_STATE_CONFIRMING: {
            uint16_t elapsed_ms = (uint16_t)((now_us - s_event_start_us) / 1000);
            uint16_t decay_ms = (uint16_t)((now_us - s_confirm_start_us) / 1000);

            if (sample >= s_threshold) {
                s_state = DETECT_STATE_TRIGGERED;
                s_energy_accum += (uint64_t)sample * sample;
            } else {
                // decay confirmed by timeout or signal drop
                if (decay_ms > KILL_DECAY_MAX_MS || sample < s_threshold / 2) {
                    emit_kill_event(s_peak_adc, elapsed_ms, s_energy_accum);
                    s_state = DETECT_STATE_IDLE;
                    s_cooldown_until_us = now_us + (KILL_COOLDOWN_MS * 1000LL);
                    s_peak_adc = 0;
                    s_energy_accum = 0;
                }
            }
            break;
        }

        default:
            s_state = DETECT_STATE_IDLE;
            break;
        }

        vTaskDelay(sample_period);
    }
}
