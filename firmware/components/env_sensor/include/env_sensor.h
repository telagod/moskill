#pragma once

#include "moskill_types.h"

void env_sensor_init(void);
void env_sensor_task(void *pvParameters);
env_reading_t env_sensor_get_latest(void);
