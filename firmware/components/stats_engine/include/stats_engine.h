#pragma once

#include "moskill_types.h"

void stats_engine_init(void);
void stats_engine_task(void *pvParameters);

const session_stats_t *stats_get_session(void);
const lifetime_stats_t *stats_get_lifetime(void);
int stats_get_kill_log(kill_event_t *buf, int page, int *total_pages);
void stats_reset_session(void);
