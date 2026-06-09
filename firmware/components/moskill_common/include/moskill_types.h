#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    KILL_CLASS_S = 0,   // fruit fly, gnat
    KILL_CLASS_M = 1,   // mosquito
    KILL_CLASS_L = 2,   // house fly, moth
    KILL_CLASS_XL = 3,  // large moth, beetle
    KILL_CLASS_COUNT = 4
} kill_class_t;

typedef enum {
    DETECT_STATE_IDLE = 0,
    DETECT_STATE_TRIGGERED,
    DETECT_STATE_CONFIRMING,
    DETECT_STATE_KILL_EVENT,
    DETECT_STATE_COOLDOWN
} detect_state_t;

typedef struct {
    uint32_t timestamp;
    uint8_t kill_class;
    uint16_t peak_adc;
    uint16_t duration_ms;
    uint32_t energy_proxy;
    int8_t temperature;
    uint8_t humidity;
} kill_event_t;

typedef struct __attribute__((packed)) {
    uint32_t timestamp;
    uint8_t kill_class;
    uint16_t peak_adc;
    uint16_t duration_ms;
    uint32_t energy_proxy;
    int8_t temperature;
    uint8_t humidity;
} kill_event_wire_t;  // 15 bytes, packed for BLE transfer

typedef struct {
    uint32_t session_id;
    uint32_t start_time;
    uint32_t duration_sec;
    uint16_t kills_total;
    uint16_t kills_by_class[KILL_CLASS_COUNT];
    uint16_t max_streak;
    uint16_t current_streak;
    float avg_temp;
    float avg_humidity;
    uint32_t energy_total;
    uint16_t hv_on_time_sec;
    uint16_t efficiency_score;
} session_stats_t;

typedef struct {
    uint32_t total_kills;
    uint32_t kills_by_class[KILL_CLASS_COUNT];
    uint32_t total_sessions;
    uint32_t total_active_sec;
    uint32_t best_streak;
    uint32_t best_session_kills;
    uint32_t kill_rate_x100;
    uint8_t hourly_histogram[24];
    uint8_t daily_histogram[7];
    uint32_t first_use_time;
} lifetime_stats_t;

typedef struct __attribute__((packed)) {
    uint32_t session_id;
    uint32_t start_time;
    uint32_t duration_sec;
    uint16_t kills_total;
    uint16_t kills_S;
    uint16_t kills_M;
    uint16_t kills_L;
    uint16_t kills_XL;
    uint16_t max_streak;
    uint32_t energy_total;
    int16_t avg_temp_x10;
    uint16_t avg_humidity_x10;
    uint16_t hv_on_time_sec;
    uint16_t efficiency_x100;
} session_stats_wire_t;  // 36 bytes

typedef struct __attribute__((packed)) {
    uint32_t total_kills;
    uint32_t kills_S;
    uint32_t kills_M;
    uint32_t kills_L;
    uint32_t kills_XL;
    uint32_t total_sessions;
    uint32_t total_active_sec;
    uint32_t best_streak;
    uint32_t best_session_kills;
    uint32_t kill_rate_x100;
    uint8_t hourly_histogram[24];
    uint8_t daily_histogram[7];
    uint8_t padding;
    uint32_t first_use_time;
} lifetime_stats_wire_t;  // 76 bytes

typedef struct {
    uint8_t kill_sensitivity;   // 0=low, 1=med, 2=high
    uint8_t led_brightness;
    uint8_t buzzer_volume;      // 0=off, 1-3
    bool buzzer_on_kill;
    bool led_on_kill;
    bool streak_effects;
} device_config_t;

typedef struct {
    float temperature;
    float humidity;
    bool valid;
} env_reading_t;

typedef struct {
    uint16_t voltage_mv;
    uint8_t percentage;
    bool charging;
    bool low;
} battery_status_t;
