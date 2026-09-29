#include "dht_11.h"
#include "dht.h"
#include "esp_err.h"
#include "esp_log.h"
#include "sensor_data.h"
#include "soc/gpio_num.h"
#include "sdkconfig.h"

static const char *TAG = "DHT11";

static dht_sensor_type_t sensor_type = DHT_TYPE_DHT11;
static gpio_num_t dht_11_data_pin = (gpio_num_t)CONFIG_APP_DHT_PIN_NUMBER;

esp_err_t dht_11_read(sensor_data_t *sensor_data){
    if (sensor_data == NULL){
        ESP_LOGE(TAG, "sensor_data is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    float humidity;
    float temperature;

    esp_err_t ret = dht_read_float_data(sensor_type, dht_11_data_pin, &humidity, &temperature);
    if (ret != ESP_OK){
        ESP_LOGE(TAG, "DHT reading failed: %s", esp_err_to_name(ret));
        return ret;
    }
    sensor_data->humidity = humidity;
    sensor_data->temperature = temperature;

    ESP_LOGI(TAG, "DHT reading successful, Humidity: %.f, Temperature: %.1f",
        humidity, temperature);
    return ESP_OK;
}
