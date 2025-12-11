#ifndef DHT11_SENSOR_H
#define DHT11_SENSOR_H
#include <stdint.h>
#include "sensor_base.h"
#include "dht.h"

typedef struct dht11_context
{
    gpio_num_t pin;
    float humidity; 
    float temperature;
} dht11_context_t;

SensorStatus_t dht11_init(sensor_base_t *self, uint8_t pin);
SensorStatus_t dht11_read_data(sensor_base_t *self, void *data);
SensorStatus_t dht11_deinit(sensor_base_t *self);
#endif // DHT11_SENSOR_H