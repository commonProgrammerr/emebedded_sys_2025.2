#include "dht11_sensor.h"
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "DHT11";

SensorStatus_t dht11_init(sensor_base_t *self, uint8_t pin)
{
    if (self == NULL)
        ESP_LOGE(TAG, "Invalid address for sensor");

    if (self->context != NULL)
        free(self->context);

    self->context = malloc(sizeof(dht11_context_t));
    if (self->context == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for context");
        return SENSOR_ERROR;
    }

    ((dht11_context_t *)self->context)->pin = pin;
    ((dht11_context_t *)self->context)->humidity = 0.0f;
    ((dht11_context_t *)self->context)->temperature = 0.0f;
    
    self->read_data = dht11_read_data;
    self->deinit = dht11_deinit;

    ESP_LOGD(TAG, "DHT11 sensor initialized successfully on pin %d", pin);
    return SENSOR_OK;
}

SensorStatus_t dht11_read_data(sensor_base_t *self, void *data)
{
    if (self == NULL || data == NULL)
    {
        ESP_LOGE(TAG, "Invalid parameters");
        return SENSOR_ERROR;
    }
    dht11_context_t *context = (dht11_context_t *)self->context;
    esp_err_t ret;
    ret = dht_read_float_data(DHT_TYPE_DHT11, context->pin, &context->humidity, &context->temperature);
    if (ret == ESP_OK)
    {
        memcpy(data, context, sizeof(dht11_context_t));
        ESP_LOGI(TAG, "Temperature: %.2f°C, Humidity: %.2f%%", context->temperature, context->humidity);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to read DHT11 data: %s", esp_err_to_name(ret));
        return SENSOR_ERROR;
    }

    return SENSOR_OK;
}

SensorStatus_t dht11_deinit(sensor_base_t *self)
{
    if (self == NULL)
    {
        return SENSOR_ERROR;
    }

    // Liberar memória
    if (self->context != NULL)
    {
        free(self->context);
    }
    free(self);

    ESP_LOGD(TAG, "DHT11 sensor deinitialized");
    return SENSOR_OK;
}