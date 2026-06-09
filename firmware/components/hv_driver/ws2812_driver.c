#include "ws2812_driver.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <math.h>

static const char *TAG = "ws2812";

#define WS2812_T0H_NS  350
#define WS2812_T0L_NS  900
#define WS2812_T1H_NS  900
#define WS2812_T1L_NS  350
#define WS2812_RESET_NS 50000

#define MAX_LEDS 8

static rmt_channel_handle_t s_channel = NULL;
static rmt_encoder_handle_t s_encoder = NULL;
static ws2812_color_t s_leds[MAX_LEDS];
static int s_num_leds = 0;
static uint8_t s_brightness = 255;

typedef struct {
    rmt_encoder_t base;
    rmt_encoder_t *bytes_encoder;
    rmt_encoder_t *copy_encoder;
    int state;
    rmt_symbol_word_t reset_code;
} ws2812_encoder_t;

static size_t ws2812_encode(rmt_encoder_t *encoder, rmt_channel_handle_t channel,
                             const void *primary_data, size_t data_size,
                             rmt_encode_state_t *ret_state)
{
    ws2812_encoder_t *ws_enc = __containerof(encoder, ws2812_encoder_t, base);
    rmt_encode_state_t session_state = RMT_ENCODING_RESET;
    size_t encoded_symbols = 0;

    switch (ws_enc->state) {
    case 0:
        encoded_symbols += ws_enc->bytes_encoder->encode(ws_enc->bytes_encoder, channel,
                                                          primary_data, data_size, &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            ws_enc->state = 1;
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            *ret_state = (rmt_encode_state_t)RMT_ENCODING_MEM_FULL;
            return encoded_symbols;
        }
        // fall through
    case 1:
        encoded_symbols += ws_enc->copy_encoder->encode(ws_enc->copy_encoder, channel,
                                                         &ws_enc->reset_code, sizeof(ws_enc->reset_code),
                                                         &session_state);
        if (session_state & RMT_ENCODING_COMPLETE) {
            ws_enc->state = RMT_ENCODING_RESET;
            *ret_state = (rmt_encode_state_t)RMT_ENCODING_COMPLETE;
        }
        if (session_state & RMT_ENCODING_MEM_FULL) {
            *ret_state = (rmt_encode_state_t)RMT_ENCODING_MEM_FULL;
        }
        break;
    }
    return encoded_symbols;
}

static esp_err_t ws2812_del(rmt_encoder_t *encoder)
{
    ws2812_encoder_t *ws_enc = __containerof(encoder, ws2812_encoder_t, base);
    rmt_del_encoder(ws_enc->bytes_encoder);
    rmt_del_encoder(ws_enc->copy_encoder);
    free(ws_enc);
    return ESP_OK;
}

static esp_err_t ws2812_reset(rmt_encoder_t *encoder)
{
    ws2812_encoder_t *ws_enc = __containerof(encoder, ws2812_encoder_t, base);
    rmt_encoder_reset(ws_enc->bytes_encoder);
    rmt_encoder_reset(ws_enc->copy_encoder);
    ws_enc->state = RMT_ENCODING_RESET;
    return ESP_OK;
}

static esp_err_t create_encoder(rmt_encoder_handle_t *ret_encoder, uint32_t resolution)
{
    ws2812_encoder_t *ws_enc = calloc(1, sizeof(ws2812_encoder_t));
    if (!ws_enc) return ESP_ERR_NO_MEM;

    ws_enc->base.encode = ws2812_encode;
    ws_enc->base.del = ws2812_del;
    ws_enc->base.reset = ws2812_reset;

    rmt_bytes_encoder_config_t bytes_cfg = {
        .bit0 = {
            .duration0 = WS2812_T0H_NS * resolution / 1000000000,
            .level0 = 1,
            .duration1 = WS2812_T0L_NS * resolution / 1000000000,
            .level1 = 0,
        },
        .bit1 = {
            .duration0 = WS2812_T1H_NS * resolution / 1000000000,
            .level0 = 1,
            .duration1 = WS2812_T1L_NS * resolution / 1000000000,
            .level1 = 0,
        },
        .flags.msb_first = 1,
    };
    ESP_ERROR_CHECK(rmt_new_bytes_encoder(&bytes_cfg, &ws_enc->bytes_encoder));

    rmt_copy_encoder_config_t copy_cfg = {};
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&copy_cfg, &ws_enc->copy_encoder));

    uint32_t reset_ticks = WS2812_RESET_NS * resolution / 1000000000;
    ws_enc->reset_code = (rmt_symbol_word_t){
        .duration0 = reset_ticks / 2,
        .level0 = 0,
        .duration1 = reset_ticks / 2,
        .level1 = 0,
    };

    *ret_encoder = &ws_enc->base;
    return ESP_OK;
}

void ws2812_init(int gpio_num, int num_leds)
{
    if (num_leds > MAX_LEDS) num_leds = MAX_LEDS;
    s_num_leds = num_leds;
    memset(s_leds, 0, sizeof(s_leds));

    uint32_t resolution = 10000000; // 10MHz → 100ns per tick

    rmt_tx_channel_config_t tx_cfg = {
        .gpio_num = gpio_num,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = resolution,
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_cfg, &s_channel));
    ESP_ERROR_CHECK(create_encoder(&s_encoder, resolution));
    ESP_ERROR_CHECK(rmt_enable(s_channel));

    ESP_LOGI(TAG, "WS2812 initialized: gpio=%d leds=%d", gpio_num, num_leds);
}

void ws2812_set(int index, ws2812_color_t color)
{
    if (index < 0 || index >= s_num_leds) return;
    s_leds[index].r = (uint8_t)((uint16_t)color.r * s_brightness / 255);
    s_leds[index].g = (uint8_t)((uint16_t)color.g * s_brightness / 255);
    s_leds[index].b = (uint8_t)((uint16_t)color.b * s_brightness / 255);
}

void ws2812_show(void)
{
    // WS2812B expects GRB order
    uint8_t buf[MAX_LEDS * 3];
    for (int i = 0; i < s_num_leds; i++) {
        buf[i * 3 + 0] = s_leds[i].g;
        buf[i * 3 + 1] = s_leds[i].r;
        buf[i * 3 + 2] = s_leds[i].b;
    }

    rmt_transmit_config_t tx_cfg = { .loop_count = 0 };
    rmt_transmit(s_channel, s_encoder, buf, s_num_leds * 3, &tx_cfg);
    rmt_tx_wait_all_done(s_channel, pdMS_TO_TICKS(100));
}

void ws2812_clear(void)
{
    memset(s_leds, 0, sizeof(s_leds));
    ws2812_show();
}

void ws2812_breathe(ws2812_color_t color, int period_ms, int step_ms)
{
    int steps = period_ms / step_ms;
    for (int i = 0; i < steps; i++) {
        float phase = (float)i / steps * 2.0f * 3.14159f;
        float brightness = (sinf(phase) + 1.0f) / 2.0f;
        ws2812_color_t c = {
            .r = (uint8_t)(color.r * brightness),
            .g = (uint8_t)(color.g * brightness),
            .b = (uint8_t)(color.b * brightness),
        };
        ws2812_set(0, c);
        ws2812_show();
        vTaskDelay(pdMS_TO_TICKS(step_ms));
    }
}

void ws2812_rainbow_chase(int speed_ms)
{
    static uint8_t hue = 0;
    for (int i = 0; i < s_num_leds; i++) {
        uint8_t h = hue + i * (256 / s_num_leds);
        uint8_t region = h / 43;
        uint8_t remainder = (h - region * 43) * 6;
        uint8_t p = 0, q = 255 - remainder, t = remainder;
        ws2812_color_t c;
        switch (region) {
            case 0: c = (ws2812_color_t){255, t, p}; break;
            case 1: c = (ws2812_color_t){q, 255, p}; break;
            case 2: c = (ws2812_color_t){p, 255, t}; break;
            case 3: c = (ws2812_color_t){p, q, 255}; break;
            case 4: c = (ws2812_color_t){t, p, 255}; break;
            default: c = (ws2812_color_t){255, p, q}; break;
        }
        ws2812_set(i, c);
    }
    ws2812_show();
    hue += 8;
    vTaskDelay(pdMS_TO_TICKS(speed_ms));
}
