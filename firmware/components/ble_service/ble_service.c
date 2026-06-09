#include "ble_service.h"
#include "ota_handler.h"
#include "moskill_config.h"
#include "moskill_types.h"
#include "stats_engine.h"
#include "kill_detector.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"

#include <string.h>
#include <sys/time.h>

extern void kill_detector_set_sensitivity(uint8_t level);

static const char *TAG = "ble";

// 128-bit UUIDs: base e3a1XXXX-f5e8-4c8a-9b3d-2c1f7b8a6d50 (little-endian byte order)
#define MOSKILL_BASE_UUID(x) {0x50,0x6d,0x8a,0x7b,0x1f,0x2c,0x3d,0x9b,0x8a,0x4c,0xe8,0xf5, (x)&0xFF,((x)>>8)&0xFF, 0xa1,0xe3}
static const uint8_t moskill_svc_uuid128[]   = MOSKILL_BASE_UUID(0x1B00);
static const uint8_t kill_count_uuid128[]    = MOSKILL_BASE_UUID(0x1B01);
static const uint8_t session_uuid128[]       = MOSKILL_BASE_UUID(0x1B02);
static const uint8_t lifetime_uuid128[]      = MOSKILL_BASE_UUID(0x1B03);
static const uint8_t kill_log_uuid128[]      = MOSKILL_BASE_UUID(0x1B04);
static const uint8_t env_uuid128[]           = MOSKILL_BASE_UUID(0x1B05);
static const uint8_t config_uuid128[]        = MOSKILL_BASE_UUID(0x1B11);
static const uint8_t time_sync_uuid128[]     = MOSKILL_BASE_UUID(0x1B12);
static const uint8_t ota_ctrl_uuid128[]      = MOSKILL_BASE_UUID(0x1B21);

#define GATTS_APP_ID            0
#define PROFILE_NUM             1

struct gatts_profile {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    bool connected;
    bool kill_notify_enabled;
    bool env_notify_enabled;
    bool bat_notify_enabled;
};

enum {
    IDX_SVC,
    IDX_KILL_COUNT_CHAR,
    IDX_KILL_COUNT_VAL,
    IDX_KILL_COUNT_CCCD,
    IDX_SESSION_CHAR,
    IDX_SESSION_VAL,
    IDX_LIFETIME_CHAR,
    IDX_LIFETIME_VAL,
    IDX_KILL_LOG_CHAR,
    IDX_KILL_LOG_VAL,
    IDX_ENV_CHAR,
    IDX_ENV_VAL,
    IDX_ENV_CCCD,
    IDX_CONFIG_CHAR,
    IDX_CONFIG_VAL,
    IDX_TIME_SYNC_CHAR,
    IDX_TIME_SYNC_VAL,
    IDX_OTA_CTRL_CHAR,
    IDX_OTA_CTRL_VAL,
    IDX_OTA_CTRL_CCCD,
    IDX_TABLE_SIZE,
};

static struct gatts_profile s_profile;
static uint16_t s_handle_table[IDX_TABLE_SIZE];

static const uint16_t primary_svc_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t char_decl_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t cccd_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

static const uint8_t char_prop_read = ESP_GATT_CHAR_PROP_BIT_READ;
static const uint8_t char_prop_read_notify = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint8_t char_prop_read_write = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE;
static const uint8_t char_prop_write = ESP_GATT_CHAR_PROP_BIT_WRITE;
static const uint8_t char_prop_write_notify = ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY;


static uint32_t s_kill_count_val = 0;
static uint16_t s_kill_cccd_val = 0;
static uint16_t s_env_cccd_val = 0;
static uint16_t s_ota_cccd_val = 0;
static device_config_t s_device_config = {
    .kill_sensitivity = 1,
    .led_brightness = 128,
    .buzzer_volume = 2,
    .buzzer_on_kill = true,
    .led_on_kill = true,
    .streak_effects = true,
};

static const esp_gatts_attr_db_t gatt_db[IDX_TABLE_SIZE] = {
    [IDX_SVC] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&primary_svc_uuid, ESP_GATT_PERM_READ,
         16, 16, (uint8_t *)moskill_svc_uuid128}
    },
    [IDX_KILL_COUNT_CHAR] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&char_decl_uuid, ESP_GATT_PERM_READ,
         1, 1, (uint8_t *)&char_prop_read_notify}
    },
    [IDX_KILL_COUNT_VAL] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_128, (uint8_t *)kill_count_uuid128, ESP_GATT_PERM_READ,
         sizeof(uint32_t), sizeof(uint32_t), (uint8_t *)&s_kill_count_val}
    },
    [IDX_KILL_COUNT_CCCD] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&cccd_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
         sizeof(uint16_t), sizeof(uint16_t), (uint8_t *)&s_kill_cccd_val}
    },
    [IDX_SESSION_CHAR] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&char_decl_uuid, ESP_GATT_PERM_READ,
         1, 1, (uint8_t *)&char_prop_read}
    },
    [IDX_SESSION_VAL] = {
        {ESP_GATT_RSP_BY_APP},
        {ESP_UUID_LEN_128, (uint8_t *)session_uuid128, ESP_GATT_PERM_READ,
         sizeof(session_stats_t), 0, NULL}
    },
    [IDX_LIFETIME_CHAR] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&char_decl_uuid, ESP_GATT_PERM_READ,
         1, 1, (uint8_t *)&char_prop_read}
    },
    [IDX_LIFETIME_VAL] = {
        {ESP_GATT_RSP_BY_APP},
        {ESP_UUID_LEN_128, (uint8_t *)lifetime_uuid128, ESP_GATT_PERM_READ,
         sizeof(lifetime_stats_t), 0, NULL}
    },
    [IDX_KILL_LOG_CHAR] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&char_decl_uuid, ESP_GATT_PERM_READ,
         1, 1, (uint8_t *)&char_prop_read}
    },
    [IDX_KILL_LOG_VAL] = {
        {ESP_GATT_RSP_BY_APP},
        {ESP_UUID_LEN_128, (uint8_t *)kill_log_uuid128, ESP_GATT_PERM_READ,
         200, 0, NULL}
    },
    [IDX_ENV_CHAR] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&char_decl_uuid, ESP_GATT_PERM_READ,
         1, 1, (uint8_t *)&char_prop_read_notify}
    },
    [IDX_ENV_VAL] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_128, (uint8_t *)env_uuid128, ESP_GATT_PERM_READ,
         4, 0, NULL}
    },
    [IDX_ENV_CCCD] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&cccd_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
         sizeof(uint16_t), sizeof(uint16_t), (uint8_t *)&s_env_cccd_val}
    },
    [IDX_CONFIG_CHAR] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&char_decl_uuid, ESP_GATT_PERM_READ,
         1, 1, (uint8_t *)&char_prop_read_write}
    },
    [IDX_CONFIG_VAL] = {
        {ESP_GATT_RSP_BY_APP},
        {ESP_UUID_LEN_128, (uint8_t *)config_uuid128, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
         sizeof(device_config_t), 0, NULL}
    },
    [IDX_TIME_SYNC_CHAR] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&char_decl_uuid, ESP_GATT_PERM_READ,
         1, 1, (uint8_t *)&char_prop_write}
    },
    [IDX_TIME_SYNC_VAL] = {
        {ESP_GATT_RSP_BY_APP},
        {ESP_UUID_LEN_128, (uint8_t *)time_sync_uuid128, ESP_GATT_PERM_WRITE,
         4, 0, NULL}
    },
    [IDX_OTA_CTRL_CHAR] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&char_decl_uuid, ESP_GATT_PERM_READ,
         1, 1, (uint8_t *)&char_prop_write_notify}
    },
    [IDX_OTA_CTRL_VAL] = {
        {ESP_GATT_RSP_BY_APP},
        {ESP_UUID_LEN_128, (uint8_t *)ota_ctrl_uuid128, ESP_GATT_PERM_WRITE,
         244, 0, NULL}
    },
    [IDX_OTA_CTRL_CCCD] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t *)&cccd_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
         sizeof(uint16_t), sizeof(uint16_t), (uint8_t *)&s_ota_cccd_val}
    },
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x140,  // 200ms
    .adv_int_max = 0x140,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&adv_params);
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(TAG, "Advertising started");
        }
        break;
    default:
        break;
    }
}

static void handle_read_event(esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    esp_gatt_rsp_t rsp;
    memset(&rsp, 0, sizeof(rsp));
    rsp.attr_value.handle = param->read.handle;

    if (param->read.handle == s_handle_table[IDX_SESSION_VAL]) {
        const session_stats_t *s = stats_get_session();
        session_stats_wire_t w = {
            .session_id = s->session_id, .start_time = s->start_time,
            .duration_sec = s->duration_sec, .kills_total = s->kills_total,
            .kills_S = s->kills_by_class[0], .kills_M = s->kills_by_class[1],
            .kills_L = s->kills_by_class[2], .kills_XL = s->kills_by_class[3],
            .max_streak = s->max_streak, .energy_total = s->energy_total,
            .avg_temp_x10 = (int16_t)(s->avg_temp * 10),
            .avg_humidity_x10 = (uint16_t)(s->avg_humidity * 10),
            .hv_on_time_sec = s->hv_on_time_sec,
            .efficiency_x100 = s->efficiency_score,
        };
        rsp.attr_value.len = sizeof(session_stats_wire_t);
        memcpy(rsp.attr_value.value, &w, sizeof(w));
    } else if (param->read.handle == s_handle_table[IDX_LIFETIME_VAL]) {
        const lifetime_stats_t *l = stats_get_lifetime();
        lifetime_stats_wire_t w = {
            .total_kills = l->total_kills,
            .kills_S = l->kills_by_class[0], .kills_M = l->kills_by_class[1],
            .kills_L = l->kills_by_class[2], .kills_XL = l->kills_by_class[3],
            .total_sessions = l->total_sessions, .total_active_sec = l->total_active_sec,
            .best_streak = l->best_streak, .best_session_kills = l->best_session_kills,
            .kill_rate_x100 = l->kill_rate_x100, .padding = 0,
            .first_use_time = l->first_use_time,
        };
        memcpy(w.hourly_histogram, l->hourly_histogram, 24);
        memcpy(w.daily_histogram, l->daily_histogram, 7);
        rsp.attr_value.len = sizeof(lifetime_stats_wire_t);
        memcpy(rsp.attr_value.value, &w, sizeof(w));
    } else if (param->read.handle == s_handle_table[IDX_KILL_LOG_VAL]) {
        kill_event_t buf[KILL_LOG_PAGE_SIZE];
        int total_pages;
        int page = param->read.offset;
        int count = stats_get_kill_log(buf, page, &total_pages);

        uint8_t *p = rsp.attr_value.value;
        p[0] = (uint8_t)page;
        p[1] = (uint8_t)total_pages;
        p[2] = (uint8_t)(count & 0xFF);
        p[3] = (uint8_t)(count >> 8);
        for (int i = 0; i < count; i++) {
            kill_event_wire_t w = {
                .timestamp = buf[i].timestamp, .kill_class = buf[i].kill_class,
                .peak_adc = buf[i].peak_adc, .duration_ms = buf[i].duration_ms,
                .energy_proxy = buf[i].energy_proxy,
                .temperature = buf[i].temperature, .humidity = buf[i].humidity,
            };
            memcpy(p + 4 + i * sizeof(kill_event_wire_t), &w, sizeof(w));
        }
        rsp.attr_value.len = 4 + count * sizeof(kill_event_wire_t);
    } else if (param->read.handle == s_handle_table[IDX_CONFIG_VAL]) {
        rsp.attr_value.len = sizeof(device_config_t);
        memcpy(rsp.attr_value.value, &s_device_config, sizeof(device_config_t));
    }

    esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id,
                                 ESP_GATT_OK, &rsp);
}

static void handle_write_event(esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    if (param->write.handle == s_handle_table[IDX_KILL_COUNT_CCCD]) {
        uint16_t val = param->write.value[0] | (param->write.value[1] << 8);
        s_profile.kill_notify_enabled = (val == 0x0001);
        ESP_LOGI(TAG, "Kill notify %s", s_profile.kill_notify_enabled ? "enabled" : "disabled");
    } else if (param->write.handle == s_handle_table[IDX_ENV_CCCD]) {
        uint16_t val = param->write.value[0] | (param->write.value[1] << 8);
        s_profile.env_notify_enabled = (val == 0x0001);
        ESP_LOGI(TAG, "Env notify %s", s_profile.env_notify_enabled ? "enabled" : "disabled");
    } else if (param->write.handle == s_handle_table[IDX_OTA_CTRL_CCCD]) {
        // OTA notify enabled, no flag needed currently
        ESP_LOGI(TAG, "OTA CCCD written");
    } else if (param->write.handle == s_handle_table[IDX_CONFIG_VAL]) {
        if (param->write.len == sizeof(device_config_t)) {
            memcpy(&s_device_config, param->write.value, sizeof(device_config_t));
            kill_detector_set_sensitivity(s_device_config.kill_sensitivity);
            ESP_LOGI(TAG, "Config updated: sens=%u led=%u buzz=%u",
                     s_device_config.kill_sensitivity, s_device_config.led_brightness,
                     s_device_config.buzzer_volume);
        }
    } else if (param->write.handle == s_handle_table[IDX_TIME_SYNC_VAL]) {
        if (param->write.len == 4) {
            uint32_t epoch;
            memcpy(&epoch, param->write.value, 4);
            struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
            settimeofday(&tv, NULL);
            ESP_LOGI(TAG, "Time synced to %lu", epoch);
        }
    } else if (param->write.handle == s_handle_table[IDX_OTA_CTRL_VAL]) {
        ota_status_t status = ota_handle_command(param->write.value, param->write.len);
        // notify OTA status back to client
        if (s_profile.connected) {
            uint8_t rsp_byte = (uint8_t)status;
            esp_ble_gatts_send_indicate(gatts_if, param->write.conn_id,
                                         s_handle_table[IDX_OTA_CTRL_VAL],
                                         1, &rsp_byte, false);
        }
    }

    if (param->write.need_rsp) {
        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id,
                                     ESP_GATT_OK, NULL);
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                 esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
    case ESP_GATTS_REG_EVT:
        s_profile.gatts_if = gatts_if;
        esp_ble_gatts_create_attr_tab(gatt_db, gatts_if, IDX_TABLE_SIZE, 0);

        esp_ble_gap_set_device_name(BLE_DEVICE_NAME_PREFIX);

        uint8_t adv_data[] = {
            0x02, 0x01, 0x06,                        // flags
            0x03, 0x03, 0x00, 0x1B,                  // 16-bit service UUID 0x1B00
            0x08, 0x09, 'M','o','s','K','i','l','l'  // complete local name (len=8: 1 type + 7 name)
        };
        esp_ble_gap_config_adv_data_raw(adv_data, sizeof(adv_data));
        break;

    case ESP_GATTS_CREAT_ATTR_TAB_EVT:
        if (param->add_attr_tab.status == ESP_GATT_OK && param->add_attr_tab.num_handle == IDX_TABLE_SIZE) {
            memcpy(s_handle_table, param->add_attr_tab.handles, sizeof(s_handle_table));
            esp_ble_gatts_start_service(s_handle_table[IDX_SVC]);
            ESP_LOGI(TAG, "GATT service started");
        }
        break;

    case ESP_GATTS_CONNECT_EVT:
        s_profile.connected = true;
        s_profile.conn_id = param->connect.conn_id;
        ESP_LOGI(TAG, "Client connected, conn_id=%d", param->connect.conn_id);
        break;

    case ESP_GATTS_DISCONNECT_EVT:
        if (ota_is_in_progress()) {
            uint8_t abort_cmd = OTA_CMD_ABORT;
            ota_handle_command(&abort_cmd, 1);
            ESP_LOGW(TAG, "OTA aborted due to disconnect");
        }
        s_profile.connected = false;
        s_profile.kill_notify_enabled = false;
        s_profile.env_notify_enabled = false;
        s_profile.bat_notify_enabled = false;
        esp_ble_gap_start_advertising(&adv_params);
        ESP_LOGI(TAG, "Client disconnected, resuming advertising");
        break;

    case ESP_GATTS_READ_EVT:
        handle_read_event(gatts_if, param);
        break;

    case ESP_GATTS_WRITE_EVT:
        handle_write_event(gatts_if, param);
        break;

    default:
        break;
    }
}

void ble_service_init(void)
{
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    memset(&s_profile, 0, sizeof(s_profile));
    s_profile.app_id = GATTS_APP_ID;

    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(gatts_event_handler));
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_event_handler));
    ESP_ERROR_CHECK(esp_ble_gatts_app_register(GATTS_APP_ID));
    ESP_ERROR_CHECK(esp_ble_gatt_set_local_mtu(BLE_MTU_SIZE));

    ota_handler_init();

    ESP_LOGI(TAG, "BLE service initialized");
}

void ble_service_task(void *pvParameters)
{
    while (1) {
        if (s_profile.connected && s_profile.kill_notify_enabled) {
            const session_stats_t *session = stats_get_session();
            uint32_t count = session->kills_total;
            esp_ble_gatts_send_indicate(s_profile.gatts_if, s_profile.conn_id,
                                         s_handle_table[IDX_KILL_COUNT_VAL],
                                         sizeof(uint32_t), (uint8_t *)&count, false);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void ble_notify_kill_count(uint32_t count)
{
    if (!s_profile.connected || !s_profile.kill_notify_enabled) return;

    esp_ble_gatts_send_indicate(s_profile.gatts_if, s_profile.conn_id,
                                 s_handle_table[IDX_KILL_COUNT_VAL],
                                 sizeof(uint32_t), (uint8_t *)&count, false);
}

void ble_notify_environment(int16_t temp_x10, uint16_t humi_x10)
{
    if (!s_profile.connected || !s_profile.env_notify_enabled) return;

    uint8_t data[4];
    memcpy(data, &temp_x10, 2);
    memcpy(data + 2, &humi_x10, 2);

    esp_ble_gatts_send_indicate(s_profile.gatts_if, s_profile.conn_id,
                                 s_handle_table[IDX_ENV_VAL],
                                 sizeof(data), data, false);
}

void ble_notify_battery(uint8_t percentage)
{
    if (!s_profile.connected || !s_profile.bat_notify_enabled) return;
    // Standard BAS notification — handle via separate service or here
}
