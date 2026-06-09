idf_component_register(
    SRCS "power_mgmt.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_adc esp_timer freertos esp_hw_support moskill_common
    PRIV_REQUIRES ble_service stats_engine
)
