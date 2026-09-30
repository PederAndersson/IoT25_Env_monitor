#ifndef MQTT_SERVICE_H
#define MQTT_SERVICE_H

#include "esp_err.h"
#include <stdbool.h>

esp_err_t mqtt_service_start(void);
esp_err_t mqtt_service_enqueue_telemetry(const char *payload);
bool mqtt_service_is_connected(void);

#endif
