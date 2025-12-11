#ifndef BH1750FVI_SENSOR_H
#define BH1750FVI_SENSOR_H

#include <stdint.h>
#include "sensor_base.h"
// Use bh1750 lib for implementation
#include "bh1750.h"

typedef struct bh1750fvi_context
{
    i2c_master_bus_config_t i2c_bus_config;
    i2c_master_bus_handle_t bus_handle;
    bh1750_t bh1750;
} bh1750fvi_context_t;

typedef float bh1750_data_t;

SensorStatus_t bh1750fvi_init(sensor_base_t *self, uint8_t sda_io, uint8_t scl_io, uint8_t i2c_address, uint8_t mode);
SensorStatus_t bh1750fvi_read_data(sensor_base_t *self, void *data);
SensorStatus_t bh1750fvi_deinit(sensor_base_t *self);

#endif // BH1750_SENSOR_H
