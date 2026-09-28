idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES wifi mqtt_service time_sync dht_11 sensor_data telemetry_service
)