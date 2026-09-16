#include <esp_log.h>
#include "esp_check.h"
#include "esp_err.h"
#include "wifi.h"

static const char* TAG = "Env Monitor";

void app_main(void)
{

    ESP_LOGI( TAG, "IoT-sensor starting");
    esp_err_t ret;
    ret = wifi_init();
    if (ret == ESP_OK){
        ESP_LOGI(TAG, "WiFi ready");
    }
    else {
        ESP_LOGE(TAG, "WiFi connection error");
    }

}