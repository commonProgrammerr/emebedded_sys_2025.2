#ifndef DHT11_SENSOR_H
#define DHT11_SENSOR_H
#include <stdint.h>
#include "sensor_base.h"
#include "esp32-dht11.h"

typedef struct dht11_context
{
    dht11_t sensor;
    uint32_t timeout;
} dht11_context_t;

SensorStatus_t dht11_init(sensor_base_t *self, uint8_t pin, uint32_t timeout);
SensorStatus_t dht11_read_data(sensor_base_t *self, void *data);
SensorStatus_t dht11_deinit(sensor_base_t *self);
#endif // DHT11_SENSOR_H