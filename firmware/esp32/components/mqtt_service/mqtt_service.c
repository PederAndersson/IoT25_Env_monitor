#include "mqtt_service.h"
#include "esp_err.h"
#include "esp_event_base.h"
#include "esp_log.h"
#include <mqtt_client.h>
#include <stdint.h>
#include <sdkconfig.h>

static const char * TAG = "MQTT-service";

static esp_mqtt_client_handle_t mqtt_client;

extern const uint8_t server_cert_pem_start[] asm("_binary_ca_crt_start");


static void mqtt_event_handler(void* handler_args, esp_event_base_t event_base, int32_t event_id, void* event_data){
    (void)handler_args;
    (void)event_base;

    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch(event_id){
        case MQTT_EVENT_CONNECTED:{
            ESP_LOGI(TAG, "MQTT client started");
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
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_APP_MQTT_BROKER_URI,
        .broker.verification.certificate = (const char*)server_cert_pem_start,
        .credentials.username = CONFIG_APP_MQTT_USERNAME,
        .credentials.authentication.password = CONFIG_APP_MQTT_PASSWORD
    };

    esp_err_t ret;
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