idf_component_register(
    SRCS "mqtt_service.c"
    REQUIRES esp_common
    PRIV_REQUIRES mqtt mbedtls
    INCLUDE_DIRS "include"
)