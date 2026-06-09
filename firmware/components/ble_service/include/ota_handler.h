#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

typedef enum {
    OTA_CMD_START  = 0x01,
    OTA_CMD_WRITE  = 0x02,
    OTA_CMD_VERIFY = 0x03,
    OTA_CMD_ABORT  = 0x04,
} ota_cmd_t;

typedef enum {
    OTA_STATUS_OK          = 0x00,
    OTA_STATUS_CRC_ERROR   = 0x01,
    OTA_STATUS_FLASH_ERROR = 0x02,
    OTA_STATUS_SIZE_ERROR  = 0x03,
    OTA_STATUS_ABORT       = 0xFF,
} ota_status_t;

void ota_handler_init(void);
ota_status_t ota_handle_command(const uint8_t *data, size_t len);
bool ota_is_in_progress(void);
