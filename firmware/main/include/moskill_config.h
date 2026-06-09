#pragma once

// -- Hardware Pins --
#define PIN_CURRENT_SENSE   0   // ADC channel 0
#define PIN_BATTERY_ADC     1   // ADC channel 1
#define PIN_I2C_SDA         4
#define PIN_I2C_SCL         5
#define PIN_HV_ENABLE       6
#define PIN_LED_STATUS      7
#define PIN_BUZZER          8
#define PIN_BUTTON          9
#define PIN_CHARGE_STATUS   10

// -- ADC --
#define ADC_SAMPLE_RATE_HZ  1000
#define ADC_ATTEN           3       // ADC_ATTEN_DB_11
#define ADC_WIDTH           4       // ADC_BITWIDTH_12 (ESP-IDF v5.x)
#define ADC_MAX_VALUE       4095

// -- Kill Detection Thresholds --
#define KILL_THRESHOLD_LOW      200
#define KILL_THRESHOLD_HIGH     3800
#define KILL_MIN_SUSTAIN_MS     5
#define KILL_MAX_DURATION_MS    500
#define KILL_COOLDOWN_MS        200
#define KILL_RISE_MAX_MS        2
#define KILL_DECAY_MAX_MS       100

// -- Kill Classification (ADC peak ranges) --
#define KILL_CLASS_S_MAX    800
#define KILL_CLASS_M_MAX    2000
#define KILL_CLASS_L_MAX    3200

// -- Sensitivity Presets --
#define SENSITIVITY_LOW     300
#define SENSITIVITY_MED     200
#define SENSITIVITY_HIGH    120

// -- Battery --
#define BAT_VOLTAGE_DIVIDER_RATIO   2.0f
#define BAT_FULL_MV         4200
#define BAT_EMPTY_MV        3000
#define BAT_LOW_THRESHOLD   15

// -- Environment Sensor --
#define ENV_SAMPLE_INTERVAL_MS  30000
#define AHT20_I2C_ADDR      0x38

// -- BLE --
#define BLE_DEVICE_NAME_PREFIX  "MosKill"
#define BLE_ADV_FAST_MS     200
#define BLE_ADV_SLOW_MS     1000
#define BLE_ADV_SLOW_AFTER_MS   30000
#define BLE_MAX_BONDS       4
#define BLE_MTU_SIZE        247

// -- Stats --
#define KILL_LOG_MAX_ENTRIES     256
#define KILL_LOG_PAGE_SIZE       10
#define KILL_LOG_SHARD_SIZE      64
#define KILL_LOG_NUM_SHARDS      (KILL_LOG_MAX_ENTRIES / KILL_LOG_SHARD_SIZE)
#define STREAK_WINDOW_MS         10000
#define NVS_NAMESPACE            "moskill"

// -- UI --
#define LED_KILL_FLASH_MS        200
#define BUZZER_KILL_FREQ_HZ      2000
#define BUZZER_KILL_DURATION_MS   50
#define BUZZER_LOW_BAT_FREQ_HZ   1000

// -- Power --
#define DEEP_SLEEP_TIMEOUT_MS    300000  // 5 min no activity
#define LIGHT_SLEEP_TIMEOUT_MS   30000   // 30s no activity
