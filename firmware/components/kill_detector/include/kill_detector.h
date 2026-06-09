#pragma once

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

void kill_detector_init(QueueHandle_t event_queue);
void kill_detector_task(void *pvParameters);
void kill_detector_set_sensitivity(uint8_t level);
