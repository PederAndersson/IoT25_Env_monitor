#include "time_sync.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "freertos/FreeRTOS.h"

static const char* TAG = "NTP timesync";

esp_err_t time_sync_wait(void){
    ESP_LOGI(TAG, "Time sync starting");
    const esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_err_t ret = esp_netif_sntp_init(&config);
    if (ret != ESP_OK){
        ESP_LOGE(TAG, "SNTP failed to init %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(15000));
    if (ret != ESP_OK){
        ESP_LOGE(TAG, "Sync wait failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "System time is synched");
    return ESP_OK;
}