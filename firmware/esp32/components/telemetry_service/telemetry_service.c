#include "telemetry_service.h"
#include "esp_err.h"
#include "esp_log.h"
#include <stddef.h>
#include <stdio.h>
#include <time.h>
#include "dht_11.h"
#include "sdkconfig.h"
#include "sensor_data.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "mqtt_service.h"

static const char *TAG = "telemetry service";
static const int queue_len = CONFIG_APP_TELEMETRY_QUEUE_LEN;
static const int sensor_stack_size = 4096;
static const int sensor_task_priority = 5;
static const int mqtt_stack_size = 4096;
static const int mqtt_task_priority = 4;
static const int wait_time = 1000;
static QueueHandle_t sensor_queue;
static TaskHandle_t sensor_task_handle;
static TaskHandle_t mqtt_task_handle;
static int dropped_readings = 0;



static esp_err_t create_timestamp(char *timestamp_buffer, size_t timestamp_buffer_size){
    if (timestamp_buffer == NULL || timestamp_buffer_size == 0){
        ESP_LOGE(TAG, "invalid timestamp buffer");
        return ESP_ERR_INVALID_ARG;
    }
    const time_t now = time(NULL);
    struct tm UTC_time;
    if(gmtime_r(&now, &UTC_time) == NULL){
        ESP_LOGE(TAG, "UTC time conversion failed");
        return ESP_ERR_INVALID_RESPONSE;
    }
    if(strftime(timestamp_buffer, timestamp_buffer_size, "%Y-%m-%dT%H:%M:%SZ", &UTC_time) == 0){
        ESP_LOGE(TAG, "Timestamp formatting failed");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t take_full_measurement(sensor_data_t *data){
    esp_err_t ret = dht_11_read(data);
    if (ret != ESP_OK){
        ESP_LOGE(TAG, "dht_11_read failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = create_timestamp(data->timestamp, sizeof(data->timestamp));
    if (ret != ESP_OK){
        ESP_LOGE(TAG, "create timestamp failed: %s", esp_err_to_name(ret));
        return ret;
    }
    return ESP_OK;
}

static esp_err_t enqueue_measurement(const sensor_data_t *measurement){
    if (measurement == NULL){
        ESP_LOGE(TAG, "Invalid data buffer.");
        return ESP_ERR_INVALID_ARG;
    }
    if (sensor_queue == NULL){
        ESP_LOGE(TAG, "Sensor queue invalid");
        return ESP_ERR_INVALID_STATE;
    }
    sensor_data_t discarded;
    BaseType_t ret;

    ret = xQueueSend(sensor_queue, measurement, 0);
    if (ret == pdPASS){
        ESP_LOGI(TAG, "Enqueue successful, Queued readings: %u of %d", (unsigned int)uxQueueMessagesWaiting(sensor_queue), CONFIG_APP_TELEMETRY_QUEUE_LEN);
        return ESP_OK;
    }
    else {
        ESP_LOGW(TAG, "Queue is full.");
        ret = xQueueReceive(sensor_queue, &discarded, 0);
        if (ret == pdTRUE){
            dropped_readings++;
            ESP_LOGI(TAG, "Oldest reading discarded: readings dropped: %d", dropped_readings);
        }
        else {
            ESP_LOGE(TAG, "Failed to remove oldest reading");
            return ESP_FAIL;
        }
    }
    ret = xQueueSend(sensor_queue, measurement, 0);
    if (ret != pdPASS){
        ESP_LOGE(TAG, "Enqueue failed");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static void sensor_task(void *param){
    (void)param;
    esp_err_t ret;
    sensor_data_t measurement;
    TickType_t last_wake_time = xTaskGetTickCount();
    TickType_t sensor_interval = pdMS_TO_TICKS(CONFIG_APP_TELEMETRY_SENSOR_READ_INTERVAL);

    while (true){
        ret = take_full_measurement(&measurement);
        if (ret == ESP_OK){
            ESP_LOGI(TAG, "Measurement successful.");
            ret = enqueue_measurement(&measurement);
            if (ret != ESP_OK){
                ESP_LOGE(TAG, "Enqueue failed: %s", esp_err_to_name(ret));
            }
        }else {
            ESP_LOGE(TAG, "Measurement failed: %s", esp_err_to_name(ret));
        }
        xTaskDelayUntil(&last_wake_time, sensor_interval);
    }
}

static void mqtt_publish_task(void *param){
    (void)param;
    sensor_data_t pending;
    char id[25];
    char payload[260];
    BaseType_t bt_ret;
    esp_err_t err_ret;
    bool has_pending = false;
    while (true){
        if (has_pending == false){
            bt_ret = xQueueReceive(sensor_queue, &pending, portMAX_DELAY);
            if (bt_ret != pdTRUE){
                ESP_LOGE(TAG, "Unexpected receive error");
                vTaskDelay(pdMS_TO_TICKS(wait_time));
                continue;
            }else {
                has_pending = true;
                ESP_LOGI(TAG, "Pending message received.");
            }
        }
        if (mqtt_service_is_connected() != true){
            ESP_LOGE(TAG, "MQTT service not connected.");
            vTaskDelay(pdMS_TO_TICKS(wait_time));
            continue;
        }
        err_ret = mqtt_service_copy_device_id(id, sizeof(id));
        if (err_ret != ESP_OK){
            ESP_LOGE(TAG, "Failed to copy id: %s", esp_err_to_name(err_ret));
            vTaskDelay(pdMS_TO_TICKS(wait_time));
            continue;
        }
        int written = snprintf(payload, sizeof(payload), "{\"sensorId\":\"%s\",\"timestamp\":\"%s\",\"humidity_value\":%.1f,\"humidity_unit\":\"%%\",\"temperature_value\":%.1f,\"temperature_unit\":\"C\"}",
        id, pending.timestamp, pending.humidity, pending.temperature);
        if (written < 0){
            ESP_LOGE(TAG, "Failed to format JSON");
            vTaskDelay(pdMS_TO_TICKS(wait_time));
            continue;
        }
        if ((size_t)written >= sizeof(payload)){
            ESP_LOGE(TAG, "Payload too small.");
            vTaskDelay(pdMS_TO_TICKS(wait_time));
            continue;
        }
        err_ret = mqtt_service_enqueue_telemetry(payload);
        if (err_ret == ESP_OK){
            ESP_LOGI(TAG, "Payload enqueued.");
            has_pending = false;
        }
        else{
            vTaskDelay(pdMS_TO_TICKS(wait_time));
        }
    }
}

esp_err_t telemetry_service_start(void){
    if (sensor_queue != NULL){
        ESP_LOGE(TAG, "Sensor queue already exist");
        return ESP_ERR_INVALID_STATE;
    }
    sensor_queue = xQueueCreate((UBaseType_t)queue_len, sizeof(sensor_data_t));
    if (sensor_queue == NULL){
        ESP_LOGE(TAG, "Failed to create queue");
        return ESP_ERR_NO_MEM;
    }
    BaseType_t ret = xTaskCreate(mqtt_publish_task, "mqtt publish", mqtt_stack_size, NULL, mqtt_task_priority, &mqtt_task_handle);
    if (ret != pdPASS){
        ESP_LOGE(TAG, "Failed to create MQTT task");
        mqtt_task_handle = NULL;
        vQueueDelete(sensor_queue);
        sensor_queue = NULL;
        return ESP_FAIL;
    }
    ret = xTaskCreate(sensor_task, "sensor_read", sensor_stack_size, NULL, sensor_task_priority, &sensor_task_handle);
    if (ret != pdPASS){
        ESP_LOGE(TAG, "Failed to create sensor task.");
        vTaskDelete(mqtt_task_handle);
        mqtt_task_handle = NULL;
        sensor_task_handle = NULL;
        vQueueDelete(sensor_queue);
        sensor_queue = NULL;
        return ESP_FAIL;
    }
    return ESP_OK;
}
