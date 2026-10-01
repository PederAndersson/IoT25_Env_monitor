#ifndef MQTT_SERVICE_H
#define MQTT_SERVICE_H

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>

esp_err_t mqtt_service_start(void);
esp_err_t mqtt_service_enqueue_telemetry(const char *payload);
bool mqtt_service_is_connected(void);
esp_err_t mqtt_service_copy_device_id(char *device_id, size_t device_id_size);

#endif
