idf_component_register(
    SRCS "dht_11.c"
    REQUIRES esp_common sensor_data
    PRIV_REQUIRES dht
    INCLUDE_DIRS "include"
)