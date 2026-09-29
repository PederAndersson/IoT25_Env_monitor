idf_component_register(
    SRCS "telemetry_service.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_common
    PRIV_REQUIRES sensor_data dht_11 freertos

)
