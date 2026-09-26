#ifndef DHT_11_H
#define DHT_11_H

#include "esp_err.h"
#include "sensor_data.h"



esp_err_t dht_11_read(sensor_data_t *sensor_data);

#endif