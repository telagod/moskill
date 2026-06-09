#include "env_sensor.h"
#include "moskill_config.h"
#include "ble_service.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"

static const char *TAG = "env";

static portMUX_TYPE s_env_spinlock = portMUX_INITIALIZER_UNLOCKED;
static env_reading_t s_latest = { .valid = false };
static i2c_port_t s_i2c_port = I2C_NUM_0;

static esp_err_t i2c_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    esp_err_t err = i2c_param_config(s_i2c_port, &conf);
    if (err != ESP_OK) return err;
    return i2c_driver_install(s_i2c_port, I2C_MODE_MASTER, 0, 0, 0);
}

static esp_err_t aht20_trigger_measurement(void)
{
    uint8_t cmd[] = { 0xAC, 0x33, 0x00 };
    return i2c_master_write_to_device(s_i2c_port, AHT20_I2C_ADDR, cmd, sizeof(cmd), pdMS_TO_TICKS(100));
}

static esp_err_t aht20_read(float *temperature, float *humidity)
{
    uint8_t data[7];
    esp_err_t err = i2c_master_read_from_device(s_i2c_port, AHT20_I2C_ADDR, data, sizeof(data), pdMS_TO_TICKS(100));
    if (err != ESP_OK) return err;

    if (data[0] & 0x80) return ESP_ERR_NOT_FINISHED; // busy

    uint32_t raw_humi = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | (data[3] >> 4);
    uint32_t raw_temp = (((uint32_t)data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | data[5];

    *humidity = (float)raw_humi / 1048576.0f * 100.0f;
    *temperature = (float)raw_temp / 1048576.0f * 200.0f - 50.0f;

    return ESP_OK;
}

void env_sensor_init(void)
{
    esp_err_t err = i2c_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C init failed: %s", esp_err_to_name(err));
        return;
    }

    // AHT20 soft reset
    uint8_t reset_cmd = 0xBA;
    i2c_master_write_to_device(s_i2c_port, AHT20_I2C_ADDR, &reset_cmd, 1, pdMS_TO_TICKS(100));
    vTaskDelay(pdMS_TO_TICKS(20));

    ESP_LOGI(TAG, "AHT20 initialized");
}

void env_sensor_task(void *pvParameters)
{
    while (1) {
        esp_err_t err = aht20_trigger_measurement();
        if (err == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(80)); // AHT20 measurement time

            float temp, humi;
            err = aht20_read(&temp, &humi);
            if (err == ESP_OK) {
                portENTER_CRITICAL(&s_env_spinlock);
                s_latest.temperature = temp;
                s_latest.humidity = humi;
                s_latest.valid = true;
                portEXIT_CRITICAL(&s_env_spinlock);

                ble_notify_environment((int16_t)(temp * 10), (uint16_t)(humi * 10));

                ESP_LOGD(TAG, "Temp=%.1f°C Humi=%.1f%%", temp, humi);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(ENV_SAMPLE_INTERVAL_MS));
    }
}

env_reading_t env_sensor_get_latest(void)
{
    portENTER_CRITICAL(&s_env_spinlock);
    env_reading_t copy = s_latest;
    portEXIT_CRITICAL(&s_env_spinlock);
    return copy;
}
