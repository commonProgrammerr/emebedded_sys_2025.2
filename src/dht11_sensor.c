#include "dht11_sensor.h"
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "DHT11";

typedef struct dht11_context {
    uint8_t pin;
    uint32_t timeout;
} dht11_context_t;

static SensorStatus_t dht11_init_wrapper(sensor_base_t* self) {
    dht11_context_t* context = (dht11_context_t*)self->context;
    return dht11_init(self, context->pin, context->timeout);
}

SensorStatus_t dht11_init(sensor_base_t* self, uint8_t pin, uint32_t timeout) {
    if(self == NULL) {
        self = malloc(sizeof(dht11_sensor_t));
        if(self == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for sensor");
            return SENSOR_ERROR;
        }
    }

    dht11_sensor_t* dht11 = (dht11_sensor_t*)self;
    dht11->sensor.dht11_pin = pin;
    dht11->timeout = timeout;

    if (dht11->base.context == NULL) {
        dht11->base.context = malloc(sizeof(dht11_context_t));
        if (dht11->base.context == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for context");
            free(self);
            return SENSOR_ERROR;
        }
        ((dht11_context_t*)dht11->base.context)->pin = pin;
        ((dht11_context_t*)dht11->base.context)->timeout = timeout;
    }

    dht11->base.init = dht11_init_wrapper;
    dht11->base.read_data = dht11_read_data;
    dht11->base.deinit = dht11_deinit;
    // Initialization code for DHT11 sensor can be added here
    
    ESP_LOGI(TAG, "DHT11 sensor initialized successfully on pin %d", pin);
    return SENSOR_OK;
}

SensorStatus_t dht11_read_data(sensor_base_t* self, void* data) {
    if (self == NULL || data == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return SENSOR_ERROR;
    }

    dht11_sensor_t* dht11 = (dht11_sensor_t*)self;
    dht11_t* sensor_data = (dht11_t*)data;
    
    if(!dht11_read(&dht11->sensor, dht11->timeout)) {
        memcpy(sensor_data, &dht11->sensor, sizeof(dht11_t));
        ESP_LOGI(TAG, "Temperature: %d°C, Humidity: %d%%", sensor_data->temperature, sensor_data->humidity);
    } else {
        ESP_LOGE(TAG, "Failed to read DHT11 sensor data");
        return SENSOR_ERROR;
    }
    
    return SENSOR_OK;
}

SensorStatus_t dht11_deinit(sensor_base_t* self) {
    if (self == NULL) {
        return SENSOR_ERROR;
    }

    // Liberar memória
    if (self->context != NULL) {
        free(self->context);
    }
    free(self);

    ESP_LOGI(TAG, "DHT11 sensor deinitialized");
    return SENSOR_OK;
}