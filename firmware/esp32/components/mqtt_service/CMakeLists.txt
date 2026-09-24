idf_component_register(
    SRCS "mqtt_service.c"
    REQUIRES esp_common
    PRIV_REQUIRES mqtt mbedtls esp_hw_support
    INCLUDE_DIRS "include"
)