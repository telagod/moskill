idf_component_register(
    SRCS "env_sensor.c"
    INCLUDE_DIRS "include"
    REQUIRES driver freertos moskill_common
    PRIV_REQUIRES ble_service
)
