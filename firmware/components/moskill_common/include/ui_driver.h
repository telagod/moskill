#pragma once

#include "moskill_types.h"

void ui_driver_init(void);
void ui_driver_task(void *pvParameters);

void ui_notify_kill(kill_class_t kill_class, uint16_t streak);
void ui_notify_low_battery(void);
void ui_notify_ble_connected(bool connected);
void ui_notify_charging(bool charging);
