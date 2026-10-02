#include "mqtt_service.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_event_base.h"
#include "esp_log.h"
#include <mqtt_client.h>
#include <stddef.h>
#include <stdint.h>
#include <sdkconfig.h>
#include "esp_crt_bundle.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <stdio.h>
#include <time.h>
#include "freertos/projdefs.h"
#include "portmacro.h"
#include "wifi.h"
#include "freertos/task.h"
#include "esp_random.h"

#define MQTT_CONNECTED_BIT BIT0

static const char *TAG = "MQTT-service";
static EventGroupHandle_t mqtt_event;
static esp_mqtt_client_handle_t mqtt_client;
static char client_id[25] = {0};
static char telemetry_topic[64];
static char status_topic[64];
static const char status_suffix[] = "status";
static const char telemetry_suffix[] = "telemetry";
static const int mqtt_reconnect_stack = 4096;
static const int mqtt_reconnect_priority = 3;
static const int base_mqtt_reconnect_time_ms = 10000;
static const int max_mqtt_reconnect_time_ms = 300000;
static const int wifi_check_interval_ms = 1000;
static const int mqtt_max_reconnect_tries = 5;
static const int reconnect_stable_limit_ms = 60000;
static TaskHandle_t mqtt_reconnect_handle;

bool mqtt_service_is_connected(void){
    if (mqtt_event == NULL){
        return false;
    }
    return (xEventGroupGetBits(mqtt_event) & MQTT_CONNECTED_BIT) != 0;
}

static esp_err_t create_device_id(char *id_buffer, size_t id_buffer_size){
    if(id_buffer == NULL || id_buffer_size == 0){
        ESP_LOGE(TAG, "Invalid id buffer");
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t mac[6];
    esp_err_t ret = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to get MAC address: %s", esp_err_to_name(ret));
    int written = snprintf(id_buffer, id_buffer_size, "esp32-%02x%02x%02x%02x%02x%02x", MAC2STR(mac));
    if (written < 0){
        ESP_LOGE(TAG, "Failed to format device id");
        return ESP_ERR_INVALID_RESPONSE;
    }
    if ((size_t)written >= id_buffer_size){
        ESP_LOGE(TAG, "Id buffer is too small");
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

static esp_err_t create_mqtt_topic(char *topic_buffer, size_t topic_buffer_size, const char *suffix){
    if (topic_buffer == NULL || topic_buffer_size == 0 || suffix == NULL){
        ESP_LOGE(TAG, "Invalid buffer.");
        return ESP_ERR_INVALID_ARG;
    }
    int written = snprintf(topic_buffer, topic_buffer_size, "esp-test/%s/%s", client_id,suffix);
    if (written < 0){
        ESP_LOGE(TAG, "Topic failed to format.");
        return ESP_ERR_INVALID_RESPONSE;
    }
    if ((size_t)written >= topic_buffer_size){
        ESP_LOGE(TAG, "Topic buffer is too small.");
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;

}

esp_err_t mqtt_service_enqueue_telemetry(const char *payload){
    if (payload == NULL){
        ESP_LOGE(TAG, "Invalid payload");
        return ESP_ERR_INVALID_ARG;
    }
    if (mqtt_client == NULL){
        ESP_LOGE(TAG, "MQTT client invalid");
        return ESP_ERR_INVALID_STATE;
    }
    if (mqtt_service_is_connected() == false){
        ESP_LOGE(TAG, "MQTT not connected");
        return ESP_ERR_INVALID_STATE;
    }
    int message_id = esp_mqtt_client_enqueue(mqtt_client, telemetry_topic, payload, 0, 1, false, true);
    if (message_id < 0){
        ESP_LOGE(TAG, "Message enqueue failed, message id: %d", message_id);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Message enqueue successful, message id: %d", message_id);
    return ESP_OK;
}


static void mqtt_event_handler(void* handler_args, esp_event_base_t event_base, int32_t event_id, void* event_data){
    (void)handler_args;
    (void)event_base;
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch(event_id){
        case MQTT_EVENT_CONNECTED:{
            xEventGroupSetBits(mqtt_event, MQTT_CONNECTED_BIT);
            ESP_LOGI(TAG, "connected to MQTT broker");
            if (mqtt_reconnect_handle != NULL){
                xTaskNotifyGive(mqtt_reconnect_handle);
            }
            int message_id = esp_mqtt_client_enqueue(event->client, status_topic, "online", 0, 1, true, true);
            if (message_id < 0){
                ESP_LOGE(TAG, "Failed to enqueue online status, id: %d", message_id);
            }else {
                ESP_LOGI(TAG, "Online status enqueued, message id: %d", message_id);
            }
            break;
        }
        case MQTT_EVENT_DISCONNECTED:{
            xEventGroupClearBits(mqtt_event, MQTT_CONNECTED_BIT);
            if (wifi_is_connected() == true){
                ESP_LOGW(TAG, "MQTT lost contact with broker");
            }else {
                ESP_LOGW(TAG, "MQTT Disconnected, waiting for wifi");
            }
            if (mqtt_reconnect_handle != NULL){
                xTaskNotifyGive(mqtt_reconnect_handle);
            }
            break;
        }
        case MQTT_EVENT_PUBLISHED:{
            ESP_LOGI(TAG, "Message published, id: %d", event->msg_id);
            break;
        }
        case MQTT_EVENT_ERROR:{
            ESP_LOGE(TAG, "MQTT communication error");
            break;
        }
        default:{
            ESP_LOGD(TAG, "Unhandled MQTT event: event %ld", (long)event_id);
            break;
        }
    }
}

esp_err_t mqtt_service_copy_device_id(char *device_id, size_t device_id_size){
    if (device_id == NULL || device_id_size == 0){
        ESP_LOGE(TAG, "id buffer invalid");
        return ESP_ERR_INVALID_SIZE;
    }
    if (client_id[0] == '\0'){
        ESP_LOGE(TAG, "Client id empty");
        return ESP_ERR_INVALID_STATE;
    }
    int written = snprintf(device_id, device_id_size, "%s",client_id);
    if (written < 0){
        ESP_LOGE(TAG, "Device id failed to format");
        return ESP_ERR_INVALID_RESPONSE;
    }
    if ((size_t)written >= device_id_size){
        ESP_LOGE(TAG, "Buffer size too small");
        return ESP_ERR_INVALID_SIZE;
    }

    return ESP_OK;
}

static int mqtt_service_update_reconnect_time(const int reconnect_count){
    int reconnect_time = base_mqtt_reconnect_time_ms;
    for (int i = 0; i < reconnect_count; i++){
        if (reconnect_time > max_mqtt_reconnect_time_ms / 2){
            reconnect_time = max_mqtt_reconnect_time_ms;
            break;
        }else {
            reconnect_time = reconnect_time * 2;
        }
    }
    int jitter = esp_random() % 1001;
    int total_reconnect_time_ms = reconnect_time + jitter;
    if (total_reconnect_time_ms > max_mqtt_reconnect_time_ms){
        total_reconnect_time_ms = max_mqtt_reconnect_time_ms;
    }
    return total_reconnect_time_ms;
}

static void mqtt_service_reconnect_task(void *param){
    (void)param;
    int reconnect_count = 0;

    while(true){
        uint32_t notification_count = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ESP_LOGI(TAG, "Reconnect task notified, count %lu", (unsigned long)notification_count);
        TickType_t time_start = xTaskGetTickCount();
        if (mqtt_service_is_connected() == true){
            while(true){
                TickType_t elapsed = xTaskGetTickCount() - time_start;

                if (elapsed >= pdMS_TO_TICKS(reconnect_stable_limit_ms)){
                    if (mqtt_service_is_connected() == true){
                        ESP_LOGI(TAG, "MQTT connection stable.");
                        reconnect_count = 0;
                        break;
                    }else {
                        break;
                    }
                }

                TickType_t remaining = pdMS_TO_TICKS(reconnect_stable_limit_ms) - elapsed;
                uint32_t reconnect_notification = ulTaskNotifyTake(pdTRUE, remaining);

                if (reconnect_notification > 0){
                    if (mqtt_service_is_connected() == false){
                        break;
                    }else {
                        continue;
                    }
                }
            }
        }
        while (mqtt_service_is_connected() != true){
            if (wifi_is_connected() != true){
                ESP_LOGI(TAG, "Waiting for wifi to connect");
                while (wifi_is_connected() != true){
                    vTaskDelay(pdMS_TO_TICKS(wifi_check_interval_ms));
                }
                ESP_LOGI(TAG, "Wifi connection is back");
            }
            int total_reconnect_time_ms = mqtt_service_update_reconnect_time(reconnect_count);
            ESP_LOGI(TAG, "MQTT retry in %d seconds", total_reconnect_time_ms / 1000);
            vTaskDelay(pdMS_TO_TICKS(total_reconnect_time_ms));
            if (mqtt_service_is_connected() == true){
                break;
            }
            if( wifi_is_connected() != true){
                continue;
            }
            esp_err_t ret = esp_mqtt_client_reconnect(mqtt_client);
            if (reconnect_count < mqtt_max_reconnect_tries){
                reconnect_count++;
            }
            if (ret != ESP_OK){
                ESP_LOGE(TAG, "MQTT service failed to reconnect: %s", esp_err_to_name(ret));
                continue;
            }else {
                break;
            }
        }
    }
}

esp_err_t mqtt_service_start(void){
    if (mqtt_event != NULL){
        ESP_LOGE(TAG, "MQTT event already started.");
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err_ret;

    err_ret = create_device_id(client_id, sizeof(client_id));
    ESP_RETURN_ON_ERROR(err_ret, TAG, "Failed to create id: %s", esp_err_to_name(err_ret));
    ESP_LOGI(TAG, "Device id: %s", client_id);
    err_ret = create_mqtt_topic(telemetry_topic, sizeof(telemetry_topic), telemetry_suffix);
    ESP_RETURN_ON_ERROR(err_ret, TAG, "Failed to create mqtt topic: %s", esp_err_to_name(err_ret));
    ESP_LOGI(TAG, "mqtt telemetry topic: %s", telemetry_topic);
    err_ret = create_mqtt_topic(status_topic, sizeof(status_topic), status_suffix);
    ESP_RETURN_ON_ERROR(err_ret, TAG, "failed to create status topic: %s", esp_err_to_name(err_ret));
    ESP_LOGI(TAG, "mqtt status topic: %s", status_topic);
    mqtt_event = xEventGroupCreate();
    if (mqtt_event == NULL){
        ESP_LOGE(TAG, "Failed to create MQTT eventgroup.");
        return ESP_ERR_NO_MEM;
    }
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_APP_MQTT_BROKER_URI,
        .broker.verification.crt_bundle_attach = esp_crt_bundle_attach,
        .credentials.username = CONFIG_APP_MQTT_USERNAME,
        .credentials.authentication.password = CONFIG_APP_MQTT_PASSWORD,
        .credentials.client_id = client_id,
        .session.last_will.topic = status_topic,
        .session.last_will.msg = "offline",
        .session.last_will.qos = 1,
        .session.last_will.retain = true,
        .session.keepalive = 60,
        .network.disable_auto_reconnect = true
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if(mqtt_client == NULL){
        ESP_LOGE(TAG, "Failed to create MQTT client");
        vEventGroupDelete(mqtt_event);
        mqtt_event = NULL;
        return ESP_FAIL;
    }
    err_ret = esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    if(err_ret != ESP_OK){
        ESP_LOGE(TAG, "MQTT event failed: %s", esp_err_to_name(err_ret));
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
        vEventGroupDelete(mqtt_event);
        mqtt_event = NULL;
        return err_ret;
    }
    BaseType_t bt_ret = xTaskCreate(mqtt_service_reconnect_task, "mqtt reconnect", mqtt_reconnect_stack, NULL, mqtt_reconnect_priority, &mqtt_reconnect_handle);
    if (bt_ret != pdPASS){
        ESP_LOGE(TAG, "Failed to create reconnect task.");
        mqtt_reconnect_handle = NULL;
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
        vEventGroupDelete(mqtt_event);
        mqtt_event = NULL;
        return ESP_FAIL;
    }
    err_ret = esp_mqtt_client_start(mqtt_client);
    if (err_ret != ESP_OK){
        ESP_LOGE(TAG, "MQTT client failed to start: %s", esp_err_to_name(err_ret));
        vTaskDelete(mqtt_reconnect_handle);
        mqtt_reconnect_handle = NULL;
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
        vEventGroupDelete(mqtt_event);
        mqtt_event = NULL;
        return err_ret;
    }
    ESP_LOGI(TAG, "MQTT Client started");

    return ESP_OK;
}