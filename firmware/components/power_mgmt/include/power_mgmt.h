#pragma once

#include "moskill_types.h"

void power_mgmt_init(void);
void power_mgmt_task(void *pvParameters);
battery_status_t power_get_battery(void);
void power_hv_enable(bool enable);
bool power_hv_is_enabled(void);
