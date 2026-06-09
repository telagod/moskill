#include "ota_handler.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "ota";

static esp_ota_handle_t s_ota_handle = 0;
static const esp_partition_t *s_update_partition = NULL;
static bool s_in_progress = false;
static uint32_t s_expected_size = 0;
static uint32_t s_received_size = 0;
static uint32_t s_crc32 = 0;

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len)
{
    crc = ~crc;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}

void ota_handler_init(void)
{
    s_in_progress = false;
    s_ota_handle = 0;
    s_update_partition = NULL;
    ESP_LOGI(TAG, "OTA handler initialized");
}

static ota_status_t handle_start(const uint8_t *data, size_t len)
{
    if (len < 5) return OTA_STATUS_SIZE_ERROR;

    memcpy(&s_expected_size, data + 1, 4);
    ESP_LOGI(TAG, "OTA start, expected size=%lu", s_expected_size);

    s_update_partition = esp_ota_get_next_update_partition(NULL);
    if (!s_update_partition) {
        ESP_LOGE(TAG, "No OTA partition found");
        return OTA_STATUS_FLASH_ERROR;
    }

    esp_err_t err = esp_ota_begin(s_update_partition, s_expected_size, &s_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        return OTA_STATUS_FLASH_ERROR;
    }

    s_in_progress = true;
    s_received_size = 0;
    s_crc32 = 0;

    ESP_LOGI(TAG, "OTA partition: %s", s_update_partition->label);
    return OTA_STATUS_OK;
}

static ota_status_t handle_write(const uint8_t *data, size_t len)
{
    if (!s_in_progress) return OTA_STATUS_ABORT;
    if (len < 5) return OTA_STATUS_SIZE_ERROR;

    uint32_t offset;
    memcpy(&offset, data + 1, 4);
    const uint8_t *payload = data + 5;
    size_t payload_len = len - 5;

    if (offset != s_received_size) {
        ESP_LOGW(TAG, "OTA offset mismatch: expected %lu, got %lu", s_received_size, offset);
    }

    esp_err_t err = esp_ota_write(s_ota_handle, payload, payload_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_write failed at offset %lu: %s", offset, esp_err_to_name(err));
        esp_ota_abort(s_ota_handle);
        s_in_progress = false;
        return OTA_STATUS_FLASH_ERROR;
    }

    s_crc32 = crc32_update(s_crc32, payload, payload_len);
    s_received_size += payload_len;

    if (s_received_size % 10240 == 0 || s_received_size == s_expected_size) {
        ESP_LOGI(TAG, "OTA progress: %lu/%lu (%lu%%)",
                 s_received_size, s_expected_size,
                 s_received_size * 100 / s_expected_size);
    }

    return OTA_STATUS_OK;
}

static ota_status_t handle_verify(const uint8_t *data, size_t len)
{
    if (!s_in_progress) return OTA_STATUS_ABORT;
    if (len < 5) return OTA_STATUS_SIZE_ERROR;

    uint32_t expected_crc;
    memcpy(&expected_crc, data + 1, 4);

    if (s_received_size != s_expected_size) {
        ESP_LOGE(TAG, "Size mismatch: received %lu, expected %lu", s_received_size, s_expected_size);
        esp_ota_abort(s_ota_handle);
        s_in_progress = false;
        return OTA_STATUS_SIZE_ERROR;
    }

    if (s_crc32 != expected_crc) {
        ESP_LOGE(TAG, "CRC mismatch: computed 0x%08lx, expected 0x%08lx", s_crc32, expected_crc);
        esp_ota_abort(s_ota_handle);
        s_in_progress = false;
        return OTA_STATUS_CRC_ERROR;
    }

    esp_err_t err = esp_ota_end(s_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        s_in_progress = false;
        return OTA_STATUS_FLASH_ERROR;
    }

    err = esp_ota_set_boot_partition(s_update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        s_in_progress = false;
        return OTA_STATUS_FLASH_ERROR;
    }

    ESP_LOGI(TAG, "OTA complete! CRC verified. Rebooting in 2s...");
    s_in_progress = false;

    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();

    return OTA_STATUS_OK; // won't reach here
}

static ota_status_t handle_abort(void)
{
    if (s_in_progress && s_ota_handle) {
        esp_ota_abort(s_ota_handle);
    }
    s_in_progress = false;
    s_ota_handle = 0;
    s_received_size = 0;
    ESP_LOGI(TAG, "OTA aborted");
    return OTA_STATUS_ABORT;
}

ota_status_t ota_handle_command(const uint8_t *data, size_t len)
{
    if (len < 1) return OTA_STATUS_ABORT;

    switch ((ota_cmd_t)data[0]) {
    case OTA_CMD_START:  return handle_start(data, len);
    case OTA_CMD_WRITE:  return handle_write(data, len);
    case OTA_CMD_VERIFY: return handle_verify(data, len);
    case OTA_CMD_ABORT:  return handle_abort();
    default:
        ESP_LOGW(TAG, "Unknown OTA command: 0x%02x", data[0]);
        return OTA_STATUS_ABORT;
    }
}

bool ota_is_in_progress(void)
{
    return s_in_progress;
}
