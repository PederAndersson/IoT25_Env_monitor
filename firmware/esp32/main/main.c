#include <esp_log.h>
#include "esp_err.h"
#include "wifi.h"
#include "mqtt_service.h"
#include "time_sync.h"

static const char* TAG = "Env Monitor";

void app_main(void)
{

    ESP_LOGI( TAG, "IoT-sensor starting");
    esp_err_t ret;
    ret = wifi_init();
    if (ret == ESP_OK){
        ESP_LOGI(TAG, "WiFi ready");
        ret = time_sync_wait();
        if (ret == ESP_OK){
            ESP_LOGI(TAG, "Time sync successful");
            ret = mqtt_service_start();
            if(ret == ESP_OK){
                ESP_LOGI(TAG, "MQTT client started");
            }
            else {
                ESP_LOGE(TAG, "MQTT client failed to start, %s", esp_err_to_name(ret));
            }
        }
        else {
            ESP_LOGE(TAG, "Time sync failed: %s", esp_err_to_name(ret));
        }
    }
    else {
        ESP_LOGE(TAG, "WiFi connection error: %s", esp_err_to_name(ret));
    }

}