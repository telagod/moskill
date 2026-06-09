idf_component_register(
    SRCS "ble_service.c" "ota_handler.c"
    INCLUDE_DIRS "include"
    REQUIRES bt freertos moskill_common
    PRIV_REQUIRES stats_engine kill_detector app_update esp_partition
)
