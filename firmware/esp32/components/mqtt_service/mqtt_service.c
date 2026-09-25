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
#include <stdio.h>
#include <time.h>

static const char *TAG = "MQTT-service";

static esp_mqtt_client_handle_t mqtt_client;
static const float temp_value = 23.7;
static const char value_unit = 'C';
static const char sensor_id[] = "esp32-test-temperature";
static char device_id[19];

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

static esp_err_t create_fake_payload(char *payload, size_t payload_size){
    char timestamp[21];
    if(payload == NULL || payload_size == 0){
        ESP_LOGE(TAG, "Invalid payload buffer.");
        return ESP_ERR_INVALID_ARG;
    }
    const time_t now = time(NULL);
    struct tm UTC_time;
    if(gmtime_r(&now, &UTC_time) == NULL){
        ESP_LOGE(TAG, "UTC time conversion failed");
        return ESP_ERR_INVALID_RESPONSE;
    }
    if(strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &UTC_time) == 0){
        ESP_LOGE(TAG, "Timestamp formatting failed");
        return ESP_FAIL;
    }
    int written = snprintf(payload, payload_size, "{\"sensorId\":\"%s\",\"timestamp\":\"%s\",\"value\":%.1f,\"unit\":\"%c\"}",sensor_id, timestamp, temp_value, value_unit);
    if(written < 0){
        ESP_LOGE(TAG, "JSON formatting error");
        return ESP_ERR_INVALID_RESPONSE;
    }
    if ((size_t)written >= payload_size){
        ESP_LOGE(TAG, "Payload buffer is too small.");
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void* handler_args, esp_event_base_t event_base, int32_t event_id, void* event_data){
    (void)handler_args;
    (void)event_base;

    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch(event_id){
        case MQTT_EVENT_CONNECTED:{
            ESP_LOGI(TAG, "connected to MQTT broker");
            char payload[192];
            esp_err_t ret = create_fake_payload(payload, sizeof(payload));
            if (ret != ESP_OK){
                ESP_LOGE(TAG, "Create payload failed");
            }else {
                ESP_LOGI(TAG, "Payload created: %s", payload);
                int payload_id = esp_mqtt_client_publish(event->client, CONFIG_APP_MQTT_TOPIC, payload, 0, 1, 0);
                if (payload_id < 0){
                    ESP_LOGE(TAG, "MQTT failed to publish payload");
                }else {
                    ESP_LOGI(TAG, "Payload queued, message id: %d", payload_id);
                }
            }
            break;
        }
        case MQTT_EVENT_DISCONNECTED:{
            ESP_LOGW(TAG, "Disconnected from MQTT broker");
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

esp_err_t mqtt_service_start(void){
    esp_err_t ret;
    ret = create_device_id(device_id, sizeof(device_id));
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to create id: %s", esp_err_to_name(ret));
    ESP_LOGI(TAG, "Device id: %s", device_id);
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_APP_MQTT_BROKER_URI,
        .broker.verification.crt_bundle_attach = esp_crt_bundle_attach,
        .credentials.username = CONFIG_APP_MQTT_USERNAME,
        .credentials.authentication.password = CONFIG_APP_MQTT_PASSWORD,
        .credentials.client_id = device_id
        
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if(mqtt_client == NULL){
        ESP_LOGE(TAG, "Failed to create MQTT client");
        return ESP_FAIL;
    }
    ret = esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    if(ret != ESP_OK){
        ESP_LOGE(TAG, "MQTT event failed: %s", esp_err_to_name(ret));
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
        return ret;
    }
    ret = esp_mqtt_client_start(mqtt_client);
    if (ret != ESP_OK){
        ESP_LOGE(TAG, "MQTT client failed to start: %s", esp_err_to_name(ret));
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
        return ret;
    }
    ESP_LOGI(TAG, "MQTT Client started");

    return ESP_OK;
}