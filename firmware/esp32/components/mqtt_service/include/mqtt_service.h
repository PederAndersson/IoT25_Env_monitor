#ifndef MQTT_SERVICE_H
#define MQTT_SERVICE_H

#include "esp_err.h"
#include "sensor_data.h"

esp_err_t mqtt_service_start(void);
//esp_err_t mqtt_service_publish(const sensor_data_t *data);

#endif
