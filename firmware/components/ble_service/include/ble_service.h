#pragma once

#include <stdint.h>
#include <stdbool.h>

void ble_service_init(void);
void ble_service_task(void *pvParameters);
void ble_notify_kill_count(uint32_t count);
void ble_notify_environment(int16_t temp_x10, uint16_t humi_x10);
void ble_notify_battery(uint8_t percentage);
