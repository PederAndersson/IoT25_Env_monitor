#ifndef SENSOR_DATA_H
#define SENSOR_DATA_H

typedef struct {
    float humidity;
    const char *unit;
} humidity_t;

typedef struct {
    float temperature;
    const char *unit;
} temperature_t;

typedef struct {
    humidity_t humidity;
    temperature_t temperature;
    char sensor_id[25];
    char timestamp[21];
} sensor_data_t;

#endif
