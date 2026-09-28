idf_component_register(
    SRCS "mqtt_service.c"
    REQUIRES esp_common sensor_data
    PRIV_REQUIRES mqtt mbedtls esp_hw_support dht_11
    INCLUDE_DIRS "include"
)