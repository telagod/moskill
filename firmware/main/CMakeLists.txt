idf_component_register(
    SRCS "src/main.c" "src/ui_driver.c"
    INCLUDE_DIRS "include"
    REQUIRES
        driver
        nvs_flash
        esp_adc
        esp_timer
        bt
        freertos
        moskill_common
    PRIV_REQUIRES
        kill_detector
        stats_engine
        ble_service
        env_sensor
        power_mgmt
        hv_driver
)
