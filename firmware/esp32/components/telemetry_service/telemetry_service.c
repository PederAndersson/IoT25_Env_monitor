#include "telemetry_service.h"
#include "esp_err.h"
#include "esp_log.h"
#include <time.h>
#include "dht_11.h"
#include "sdkconfig.h"
#include "sensor_data.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"


static const char *TAG = "telemetry service";
static const int queue_len = CONFIG_APP_TELEMETRY_QUEUE_LEN;
static const int stack_size = 4096;
static const int task_priority = 5;
static QueueHandle_t sensor_queue;
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
        ESP_LOGI(TAG, "Enqueue successful, Queued readings: %u of 60", (unsigned int)uxQueueMessagesWaiting(sensor_queue));
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

    while (1){
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
    BaseType_t ret = xTaskCreate(sensor_task, "sensor_read", stack_size, NULL, task_priority, NULL);
    if (ret != pdPASS){
        ESP_LOGE(TAG, "Create task failed.");
        vQueueDelete(sensor_queue);
        sensor_queue = NULL;
        return ESP_FAIL;
    }
    return ESP_OK;
}
