idf_component_register(
    SRCS "mqtt_service.c"
    REQUIRES esp_common
    PRIV_REQUIRES mqtt mbedtls esp_hw_support freertos
    INCLUDE_DIRS "include"
)
