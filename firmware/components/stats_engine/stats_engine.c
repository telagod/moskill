#include "stats_engine.h"
#include "moskill_config.h"
#include "moskill_types.h"
#include "ui_driver.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "nvs.h"
#include "esp_timer.h"

#include <string.h>
#include <stdio.h>
#include <time.h>

static const char *TAG = "stats";

extern QueueHandle_t kill_event_queue;

static session_stats_t s_session;
static lifetime_stats_t s_lifetime;
static kill_event_t s_kill_log[KILL_LOG_MAX_ENTRIES];
static int s_kill_log_head = 0;
static int s_kill_log_count = 0;
static int64_t s_last_kill_time_us = 0;
static nvs_handle_t s_nvs;
static SemaphoreHandle_t s_stats_mutex;

static void load_lifetime_stats(void)
{
    size_t len = sizeof(lifetime_stats_t);
    esp_err_t err = nvs_get_blob(s_nvs, "lifetime", &s_lifetime, &len);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        memset(&s_lifetime, 0, sizeof(s_lifetime));
        ESP_LOGI(TAG, "No lifetime stats found, starting fresh");
    } else {
        ESP_ERROR_CHECK(err);
        ESP_LOGI(TAG, "Loaded lifetime stats: %lu total kills", s_lifetime.total_kills);
    }
}

static void save_lifetime_stats(void)
{
    ESP_ERROR_CHECK(nvs_set_blob(s_nvs, "lifetime", &s_lifetime, sizeof(lifetime_stats_t)));
    ESP_ERROR_CHECK(nvs_commit(s_nvs));
}

static void load_kill_log(void)
{
    size_t meta_len = sizeof(int) * 2;
    int meta[2] = {0, 0};
    esp_err_t err = nvs_get_blob(s_nvs, "killmeta", meta, &meta_len);
    if (err == ESP_OK) {
        s_kill_log_head = meta[0];
        s_kill_log_count = meta[1];
    } else {
        s_kill_log_head = 0;
        s_kill_log_count = 0;
    }

    char key[16];
    for (int s = 0; s < KILL_LOG_NUM_SHARDS; s++) {
        snprintf(key, sizeof(key), "klog%d", s);
        size_t len = KILL_LOG_SHARD_SIZE * sizeof(kill_event_t);
        err = nvs_get_blob(s_nvs, key, &s_kill_log[s * KILL_LOG_SHARD_SIZE], &len);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            memset(&s_kill_log[s * KILL_LOG_SHARD_SIZE], 0, KILL_LOG_SHARD_SIZE * sizeof(kill_event_t));
        }
    }
    ESP_LOGI(TAG, "Kill log loaded: %d entries in %d shards", s_kill_log_count, KILL_LOG_NUM_SHARDS);
}

static void save_kill_log(void)
{
    int dirty_shard = (s_kill_log_head > 0 ? s_kill_log_head - 1 : KILL_LOG_MAX_ENTRIES - 1) / KILL_LOG_SHARD_SIZE;
    char key[16];
    snprintf(key, sizeof(key), "klog%d", dirty_shard);
    size_t len = KILL_LOG_SHARD_SIZE * sizeof(kill_event_t);
    esp_err_t err = nvs_set_blob(s_nvs, key, &s_kill_log[dirty_shard * KILL_LOG_SHARD_SIZE], len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save kill log shard %d: %s", dirty_shard, esp_err_to_name(err));
        return;
    }
    int meta[2] = { s_kill_log_head, s_kill_log_count };
    nvs_set_blob(s_nvs, "killmeta", meta, sizeof(meta));
    nvs_commit(s_nvs);
}

static void append_kill_log(const kill_event_t *evt)
{
    s_kill_log[s_kill_log_head] = *evt;
    s_kill_log_head = (s_kill_log_head + 1) % KILL_LOG_MAX_ENTRIES;
    if (s_kill_log_count < KILL_LOG_MAX_ENTRIES) s_kill_log_count++;
}

static void update_streak(int64_t now_us)
{
    if (s_last_kill_time_us > 0 &&
        (now_us - s_last_kill_time_us) < (STREAK_WINDOW_MS * 1000LL)) {
        s_session.current_streak++;
    } else {
        s_session.current_streak = 1;
    }
    if (s_session.current_streak > s_session.max_streak) {
        s_session.max_streak = s_session.current_streak;
    }
    s_last_kill_time_us = now_us;
}

static void process_kill(const kill_event_t *evt)
{
    int64_t now_us = esp_timer_get_time();

    xSemaphoreTake(s_stats_mutex, portMAX_DELAY);

    s_session.kills_total++;
    s_session.kills_by_class[evt->kill_class]++;
    s_session.energy_total += evt->energy_proxy;
    update_streak(now_us);

    s_lifetime.total_kills++;
    s_lifetime.kills_by_class[evt->kill_class]++;
    if (s_session.max_streak > s_lifetime.best_streak) {
        s_lifetime.best_streak = s_session.max_streak;
    }
    if (s_session.kills_total > s_lifetime.best_session_kills) {
        s_lifetime.best_session_kills = s_session.kills_total;
    }

    time_t ts = (time_t)evt->timestamp;
    struct tm tm_info;
    localtime_r(&ts, &tm_info);
    if (tm_info.tm_hour < 24) {
        uint16_t cur = s_lifetime.hourly_histogram[tm_info.tm_hour];
        if (cur < 255) s_lifetime.hourly_histogram[tm_info.tm_hour] = cur + 1;
    }
    if (tm_info.tm_wday < 7) {
        int dow = (tm_info.tm_wday + 6) % 7; // convert Sun=0 to Mon=0
        uint16_t cur = s_lifetime.daily_histogram[dow];
        if (cur < 255) s_lifetime.daily_histogram[dow] = cur + 1;
    }

    append_kill_log(evt);

    // persist every 10 kills to reduce flash wear
    if (s_session.kills_total % 10 == 0) {
        save_lifetime_stats();
        save_kill_log();
        ESP_LOGI(TAG, "Stats persisted (every 10 kills)");
    }

    xSemaphoreGive(s_stats_mutex);

    ui_notify_kill(evt->kill_class, s_session.current_streak);
}

void stats_engine_init(void)
{
    s_stats_mutex = xSemaphoreCreateMutex();
    assert(s_stats_mutex != NULL);

    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &s_nvs));
    load_lifetime_stats();
    load_kill_log();

    memset(&s_session, 0, sizeof(s_session));
    s_session.session_id = s_lifetime.total_sessions + 1;
    s_session.start_time = (uint32_t)(esp_timer_get_time() / 1000000);

    s_lifetime.total_sessions++;

    ESP_LOGI(TAG, "Stats engine init, session #%lu", s_session.session_id);
}

void stats_engine_task(void *pvParameters)
{
    kill_event_t evt;

    while (1) {
        if (xQueueReceive(kill_event_queue, &evt, pdMS_TO_TICKS(5000)) == pdTRUE) {
            process_kill(&evt);
        }

        xSemaphoreTake(s_stats_mutex, portMAX_DELAY);
        s_session.duration_sec = (uint32_t)(esp_timer_get_time() / 1000000) - s_session.start_time;
        xSemaphoreGive(s_stats_mutex);
    }
}

const session_stats_t *stats_get_session(void)
{
    xSemaphoreTake(s_stats_mutex, portMAX_DELAY);
    const session_stats_t *ret = &s_session;
    xSemaphoreGive(s_stats_mutex);
    return ret;
}

const lifetime_stats_t *stats_get_lifetime(void)
{
    xSemaphoreTake(s_stats_mutex, portMAX_DELAY);
    const lifetime_stats_t *ret = &s_lifetime;
    xSemaphoreGive(s_stats_mutex);
    return ret;
}

int stats_get_kill_log(kill_event_t *buf, int page, int *total_pages)
{
    xSemaphoreTake(s_stats_mutex, portMAX_DELAY);

    *total_pages = (s_kill_log_count + KILL_LOG_PAGE_SIZE - 1) / KILL_LOG_PAGE_SIZE;
    if (*total_pages == 0) *total_pages = 1;
    if (page >= *total_pages) {
        xSemaphoreGive(s_stats_mutex);
        return 0;
    }

    int start_idx = s_kill_log_count - 1 - (page * KILL_LOG_PAGE_SIZE);
    int count = 0;

    for (int i = 0; i < KILL_LOG_PAGE_SIZE && start_idx - i >= 0; i++) {
        int ring_idx = (s_kill_log_head - 1 - (page * KILL_LOG_PAGE_SIZE) - i + KILL_LOG_MAX_ENTRIES) % KILL_LOG_MAX_ENTRIES;
        buf[count++] = s_kill_log[ring_idx];
    }

    xSemaphoreGive(s_stats_mutex);
    return count;
}

void stats_reset_session(void)
{
    xSemaphoreTake(s_stats_mutex, portMAX_DELAY);

    s_lifetime.total_active_sec += s_session.duration_sec;

    save_lifetime_stats();
    save_kill_log();

    memset(&s_session, 0, sizeof(s_session));
    s_session.session_id = s_lifetime.total_sessions + 1;
    s_session.start_time = (uint32_t)(esp_timer_get_time() / 1000000);
    s_lifetime.total_sessions++;
    s_last_kill_time_us = 0;

    xSemaphoreGive(s_stats_mutex);

    ESP_LOGI(TAG, "Session reset, new session #%lu", s_session.session_id);
}
