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

#define MQTT_CONNECTED_BIT BIT0

static const char *TAG = "MQTT-service";
static EventGroupHandle_t mqtt_event;
static esp_mqtt_client_handle_t mqtt_client;
static char client_id[25] = {0};
static char telemetry_topic[64];
static char status_topic[64];
static const char status_suffix[] = "status";
static const char telemetry_suffix[] = "telemetry";

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

esp_err_t mqtt_service_start(void){
    if (mqtt_event != NULL){
        ESP_LOGE(TAG, "MQTT event already started.");
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret;

    ret = create_device_id(client_id, sizeof(client_id));
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to create id: %s", esp_err_to_name(ret));
    ESP_LOGI(TAG, "Device id: %s", client_id);
    ret = create_mqtt_topic(telemetry_topic, sizeof(telemetry_topic), telemetry_suffix);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to create mqtt topic: %s", esp_err_to_name(ret));
    ESP_LOGI(TAG, "mqtt telemetry topic: %s", telemetry_topic);
    ret = create_mqtt_topic(status_topic, sizeof(status_topic), status_suffix);
    ESP_RETURN_ON_ERROR(ret, TAG, "failed to create status topic: %s", esp_err_to_name(ret));
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
        .session.keepalive = 60
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if(mqtt_client == NULL){
        ESP_LOGE(TAG, "Failed to create MQTT client");
        vEventGroupDelete(mqtt_event);
        mqtt_event = NULL;
        return ESP_FAIL;
    }
    ret = esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    if(ret != ESP_OK){
        ESP_LOGE(TAG, "MQTT event failed: %s", esp_err_to_name(ret));
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
        vEventGroupDelete(mqtt_event);
        mqtt_event = NULL;
        return ret;
    }
    ret = esp_mqtt_client_start(mqtt_client);
    if (ret != ESP_OK){
        ESP_LOGE(TAG, "MQTT client failed to start: %s", esp_err_to_name(ret));
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
        vEventGroupDelete(mqtt_event);
        mqtt_event = NULL;
        return ret;
    }
    ESP_LOGI(TAG, "MQTT Client started");

    return ESP_OK;
}