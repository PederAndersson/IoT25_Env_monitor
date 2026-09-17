idf_component_register(
    SRCS "mqtt_service.c"
    REQUIRES esp_common
    PRIV_REQUIRES mqtt
    INCLUDE_DIRS "include"
    EMBED_TXTFILES "${PROJECT_DIR}/../../certs/ca.crt"
)