#include "ui_driver.h"
#include "moskill_config.h"
#include "power_mgmt.h"
#include "ws2812_driver.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "ui";

typedef enum {
    UI_EVT_KILL,
    UI_EVT_LOW_BAT,
    UI_EVT_BLE_CONN,
    UI_EVT_BLE_DISC,
    UI_EVT_CHARGING,
} ui_event_type_t;

typedef struct {
    ui_event_type_t type;
    uint16_t param1;
    uint16_t param2;
} ui_event_t;

static QueueHandle_t s_ui_queue;
static bool s_button_pressed = false;
static int64_t s_button_press_start = 0;

static void buzzer_init(void)
{
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = BUZZER_KILL_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_cfg);

    ledc_channel_config_t chan_cfg = {
        .gpio_num = PIN_BUZZER,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&chan_cfg);
}

static void buzzer_beep(uint32_t freq_hz, uint32_t duration_ms)
{
    ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, freq_hz);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 128);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

static void button_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_BUTTON),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
}

static void handle_button(void)
{
    bool pressed = (gpio_get_level(PIN_BUTTON) == 0);
    int64_t now = esp_timer_get_time();

    if (pressed && !s_button_pressed) {
        s_button_pressed = true;
        s_button_press_start = now;
    } else if (!pressed && s_button_pressed) {
        s_button_pressed = false;
        int64_t hold_ms = (now - s_button_press_start) / 1000;

        if (hold_ms < 500) {
            // short press: toggle HV
            bool hv = power_hv_is_enabled();
            power_hv_enable(!hv);
            buzzer_beep(hv ? 1000 : 2000, 100);
            ESP_LOGI(TAG, "Button: HV %s", hv ? "OFF" : "ON");
        } else if (hold_ms >= 3000) {
            // long press: BLE toggle
            buzzer_beep(1500, 50);
            vTaskDelay(pdMS_TO_TICKS(100));
            buzzer_beep(2000, 50);
            ESP_LOGI(TAG, "Button: BLE toggle");
        }
    }
}

static const ws2812_color_t KILL_COLORS[] = {
    [KILL_CLASS_S]  = {0, 200, 0},     // green — small
    [KILL_CLASS_M]  = {0, 255, 0},     // bright green — mosquito
    [KILL_CLASS_L]  = {255, 200, 0},   // yellow — large
    [KILL_CLASS_XL] = {255, 0, 0},     // red — extra large
};

static void handle_kill_effect(uint16_t kill_class, uint16_t streak)
{
    buzzer_beep(BUZZER_KILL_FREQ_HZ, BUZZER_KILL_DURATION_MS);

    if (kill_class < KILL_CLASS_COUNT) {
        ws2812_set(0, KILL_COLORS[kill_class]);
        ws2812_show();
    }

    if (streak >= 3) {
        for (int i = 0; i < streak && i < 5; i++) {
            vTaskDelay(pdMS_TO_TICKS(50));
            buzzer_beep(2000 + i * 200, 30);
        }
        ws2812_rainbow_chase(50);
    }

    vTaskDelay(pdMS_TO_TICKS(LED_KILL_FLASH_MS));
    ws2812_clear();
}

void ui_driver_init(void)
{
    s_ui_queue = xQueueCreate(8, sizeof(ui_event_t));
    buzzer_init();
    button_init();
    ws2812_init(PIN_LED_STATUS, 1);

    ESP_LOGI(TAG, "UI driver initialized");
}

void ui_driver_task(void *pvParameters)
{
    ui_event_t evt;

    while (1) {
        handle_button();

        if (xQueueReceive(s_ui_queue, &evt, pdMS_TO_TICKS(50)) == pdTRUE) {
            switch (evt.type) {
            case UI_EVT_KILL:
                handle_kill_effect(evt.param1, evt.param2);
                break;
            case UI_EVT_LOW_BAT:
                for (int i = 0; i < 3; i++) {
                    ws2812_set(0, WS2812_RED);
                    ws2812_show();
                    buzzer_beep(BUZZER_LOW_BAT_FREQ_HZ, 100);
                    ws2812_clear();
                    vTaskDelay(pdMS_TO_TICKS(200));
                }
                break;
            case UI_EVT_BLE_CONN:
                ws2812_set(0, WS2812_BLUE);
                ws2812_show();
                buzzer_beep(1500, 50);
                vTaskDelay(pdMS_TO_TICKS(100));
                buzzer_beep(2000, 100);
                ws2812_breathe(WS2812_BLUE, 1000, 20);
                ws2812_clear();
                break;
            case UI_EVT_BLE_DISC:
                ws2812_clear();
                break;
            case UI_EVT_CHARGING:
                ws2812_breathe(WS2812_ORANGE, 2000, 20);
                break;
            default:
                break;
            }
        }
    }
}

void ui_notify_kill(kill_class_t kill_class, uint16_t streak)
{
    ui_event_t evt = { .type = UI_EVT_KILL, .param1 = kill_class, .param2 = streak };
    xQueueSend(s_ui_queue, &evt, 0);
}

void ui_notify_low_battery(void)
{
    ui_event_t evt = { .type = UI_EVT_LOW_BAT };
    xQueueSend(s_ui_queue, &evt, 0);
}

void ui_notify_ble_connected(bool connected)
{
    ui_event_t evt = { .type = connected ? UI_EVT_BLE_CONN : UI_EVT_BLE_DISC };
    xQueueSend(s_ui_queue, &evt, 0);
}

void ui_notify_charging(bool charging)
{
    ui_event_t evt = { .type = UI_EVT_CHARGING };
    xQueueSend(s_ui_queue, &evt, 0);
}
