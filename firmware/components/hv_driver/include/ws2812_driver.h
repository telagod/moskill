#pragma once

#include <stdint.h>

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} ws2812_color_t;

void ws2812_init(int gpio_num, int num_leds);
void ws2812_set(int index, ws2812_color_t color);
void ws2812_show(void);
void ws2812_clear(void);

// preset colors
#define WS2812_RED      (ws2812_color_t){255, 0, 0}
#define WS2812_GREEN    (ws2812_color_t){0, 255, 0}
#define WS2812_BLUE     (ws2812_color_t){0, 0, 255}
#define WS2812_ORANGE   (ws2812_color_t){255, 80, 0}
#define WS2812_OFF      (ws2812_color_t){0, 0, 0}

// effects
void ws2812_breathe(ws2812_color_t color, int period_ms, int step_ms);
void ws2812_rainbow_chase(int speed_ms);
