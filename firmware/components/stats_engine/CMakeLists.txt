idf_component_register(
    SRCS "stats_engine.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash esp_timer freertos moskill_common
)
