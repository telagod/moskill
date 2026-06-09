#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "moskill_config.h"
#include "moskill_types.h"
#include "kill_detector.h"
#include "stats_engine.h"
#include "ble_service.h"
#include "power_mgmt.h"
#include "env_sensor.h"
#include "ui_driver.h"

static const char *TAG = "moskill";

QueueHandle_t kill_event_queue;

void app_main(void)
{
    ESP_LOGI(TAG, "MosKill v1.0.0 starting...");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    kill_event_queue = xQueueCreate(16, sizeof(kill_event_t));
    assert(kill_event_queue != NULL);

    stats_engine_init();
    env_sensor_init();
    ui_driver_init();
    power_mgmt_init();
    kill_detector_init(kill_event_queue);
    ble_service_init();

    xTaskCreate(kill_detector_task, "kill_det", 4096, NULL, 5, NULL);
    xTaskCreate(stats_engine_task, "stats", 8192, NULL, 4, NULL);
    xTaskCreate(ble_service_task, "ble", 6144, NULL, 3, NULL);
    xTaskCreate(env_sensor_task, "env", 2048, NULL, 2, NULL);
    xTaskCreate(ui_driver_task, "ui", 2048, NULL, 2, NULL);
    xTaskCreate(power_mgmt_task, "power", 2048, NULL, 1, NULL);

    ESP_LOGI(TAG, "All tasks launched. Entering idle.");
}
