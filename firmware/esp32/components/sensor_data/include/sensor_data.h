#ifndef SENSOR_DATA_H
#define SENSOR_DATA_H

typedef struct {
    float humidity;
    float temperature;
    char timestamp[21];
} sensor_data_t;

#endif
