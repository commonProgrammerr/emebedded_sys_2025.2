#include "dht11_sensor.h"
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "DHT11";

SensorStatus_t dht11_init(sensor_base_t *self, uint8_t pin, uint32_t timeout)
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

    ((dht11_context_t *)self->context)->sensor.dht11_pin = pin;
    ((dht11_context_t *)self->context)->timeout = timeout;

    // Configurar GPIO com pull-up interno
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

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
    dht11_t *sensor_data = (dht11_t *)data;
    context->sensor.dht11_pin = GPIO_NUM_23;
    if (!dht11_read(&(context->sensor), context->timeout))
    {
        memcpy(sensor_data, &context->sensor, sizeof(dht11_t));
        ESP_LOGI(TAG, "Temperature: %.2f°C, Humidity: %.2f%%", context->sensor.temperature, context->sensor.humidity);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to read DHT11 sensor data");
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