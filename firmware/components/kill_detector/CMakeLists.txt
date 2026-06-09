idf_component_register(
    SRCS "kill_detector.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_adc esp_timer freertos moskill_common
    PRIV_REQUIRES env_sensor power_mgmt
)
