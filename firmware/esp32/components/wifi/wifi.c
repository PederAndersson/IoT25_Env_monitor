#include "wifi.h"
#include "esp_err.h"
#include "esp_netif_ip_addr.h"
#include "esp_netif_types.h"
#include "esp_random.h"
#include "esp_wifi_default.h"
#include "esp_wifi_types_generic.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "portmacro.h"
#include"sdkconfig.h"
#include <stdint.h>

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define MAXIMUM_RETRY CONFIG_APP_WIFI_MAXIMUM_RETRY


static const char* TAG = "Wifi Station";
static EventGroupHandle_t s_wifi_event_group = NULL;
static esp_event_handler_instance_t s_wifi_handler_instance;
static esp_event_handler_instance_t s_ip_handler_instance;
static TaskHandle_t wifi_reconnect_handle;
static bool wifi_connected_once = false;
static const int wifi_task_stack_size = 4095;
static const int wifi_task_priority = 6;
static const int base_wifi_reconnect_time_ms = 1000;
static int wifi_reconnect_time_ms = 1000;
static const int max_wifi_reconnect_time_ms = 30000;

bool wifi_is_connected(void){
    if (s_wifi_event_group == NULL){
        return false;
    }
    return (xEventGroupGetBits(s_wifi_event_group) & WIFI_CONNECTED_BIT) != 0;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data){
    (void)arg;
    esp_err_t ret;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START){
        ESP_LOGI(TAG,"WiFi station started");
        ret = esp_wifi_connect();
        if (ret != ESP_OK){
            ESP_LOGE(TAG, "WiFi connect failed");
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED){
        const wifi_event_sta_disconnected_t *disconnected = (const wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGW(TAG, "WiFi disconnected, reason: %d", disconnected->reason);
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        if (wifi_reconnect_handle != NULL){
            xTaskNotifyGive(wifi_reconnect_handle);
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP){
        const ip_event_got_ip_t *ip_event = (const ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&ip_event->ip_info.ip));
        xEventGroupClearBits(s_wifi_event_group, WIFI_FAIL_BIT);
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        wifi_connected_once = true;
        wifi_reconnect_time_ms = base_wifi_reconnect_time_ms;
    }
}

static void wifi_reconnect_time_update(int *reconnect_time){

    if (*reconnect_time > max_wifi_reconnect_time_ms / 2){
        *reconnect_time = max_wifi_reconnect_time_ms;
    }else {
        *reconnect_time = *reconnect_time * 2;
    }
}

static void wifi_reconnect_task(void *param){
    (void)param;
    esp_err_t ret;
    int retry_counter = 0;
    
    while (true){
        uint32_t reconnect_notify = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if(wifi_is_connected() == true){
            continue;
        }
        ESP_LOGI(TAG, "Wifi reconnect task notified: %lu", (unsigned long)reconnect_notify);
        if (wifi_connected_once != true){
            if (retry_counter < MAXIMUM_RETRY){
                retry_counter++;
                ESP_LOGI(TAG,"Reconnect attempt %d of %d", retry_counter, MAXIMUM_RETRY);
                ret = esp_wifi_connect();
                if (ret != ESP_OK){
                    ESP_LOGE(TAG, "WiFi reconnect failed: %s", esp_err_to_name(ret));
                    xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                }
            }
            else {
                ESP_LOGE(TAG, "maximum number of retries reached");
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            }
        }else {
            int wait_time_ms = wifi_reconnect_time_ms + (esp_random() % 1001);
            if (wait_time_ms > 30000) {wait_time_ms = max_wifi_reconnect_time_ms;}
            ESP_LOGI(TAG, "Reconnect wait time: %d", wait_time_ms);
            vTaskDelay(pdMS_TO_TICKS(wait_time_ms));
            if(wifi_is_connected() == true){
                continue;
            }
            wifi_reconnect_time_update(&wifi_reconnect_time_ms);
            ret = esp_wifi_connect();
            if (ret == ESP_OK){
                ESP_LOGI(TAG, "Wifi connection attempt started.");
            }else{
                retry_counter++;
                ESP_LOGE(TAG, "WiFi reconnect failed: %s tries: %d", esp_err_to_name(ret), retry_counter);
                xTaskNotifyGive(wifi_reconnect_handle);
                continue;
            }
        }

    }
}

esp_err_t wifi_init(void){
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret ==ESP_ERR_NVS_NEW_VERSION_FOUND){
        ret = nvs_flash_erase();
        ESP_RETURN_ON_ERROR(ret, TAG, "nvs flash erase failed");
        ret = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "nvs_flash failed" );

    ret = esp_netif_init();
    ESP_RETURN_ON_ERROR(ret, TAG, "netif init failed");

    ret = esp_event_loop_create_default();
    ESP_RETURN_ON_ERROR(ret, TAG, "create event loop failed");

    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL){
        ESP_LOGE(TAG, "event group create memory error");
        return ESP_ERR_NO_MEM;
    }
    
    esp_netif_t *station = esp_netif_create_default_wifi_sta();
    if (station == NULL){
        ESP_LOGE(TAG, "failed to create WiFi station interface");
        return ESP_FAIL;
    }

    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&wifi_init_cfg);
    ESP_RETURN_ON_ERROR(ret, TAG, "WiFi driver initialization failed");

    ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, &s_wifi_handler_instance);
    ESP_RETURN_ON_ERROR(ret, TAG, "WiFi event failed to register");
    ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, &s_ip_handler_instance);
    ESP_RETURN_ON_ERROR(ret, TAG, "IP event failed to register");

    wifi_config_t wifi_cfg = {
        .sta ={
            .ssid = CONFIG_APP_WIFI_SSID,
            .password = CONFIG_APP_WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK
        }
    };

    BaseType_t bt_ret = xTaskCreate(wifi_reconnect_task, "wifi reconnect", wifi_task_stack_size, NULL, wifi_task_priority, &wifi_reconnect_handle);
    if (bt_ret != pdPASS){
        ESP_LOGE(TAG, "Failed to create Reconnect task");
        wifi_reconnect_handle = NULL;
        return ESP_FAIL;
    }

    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    ESP_RETURN_ON_ERROR(ret, TAG, "failed to set wifi mode");
    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    ESP_RETURN_ON_ERROR(ret, TAG, "failed to config wifi");
    ret = esp_wifi_start();
    ESP_RETURN_ON_ERROR(ret, TAG, "failed to start wifi");

    ESP_LOGI(TAG, "wifi_init finished and waiting for connection");

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    if (bits & WIFI_CONNECTED_BIT){
        ESP_LOGI(TAG, "connected to wifi");
        return ESP_OK;
    }
    else if (bits & WIFI_FAIL_BIT){
        ESP_LOGE(TAG, "failed to connect to wifi");
        return ESP_FAIL;
    }
    else {
        ESP_LOGE(TAG, "Unexpected error");
        return ESP_FAIL;
    }
}