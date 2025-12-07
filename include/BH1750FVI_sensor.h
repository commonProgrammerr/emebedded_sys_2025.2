#ifndef BH1750FVI_SENSOR_H
#define BH1750FVI_SENSOR_H

#include <stdint.h>
#include "sensor_base.h"
#include "driver/i2c.h"

// BH1750FVI I2C Address
#define BH1750_ADDR_LOW  0x23  // ADDR pin = LOW
#define BH1750_ADDR_HIGH 0x5C  // ADDR pin = HIGH

// BH1750FVI Commands
#define BH1750_POWER_DOWN           0x00
#define BH1750_POWER_ON             0x01
#define BH1750_RESET                0x07
#define BH1750_CONTINUOUS_H_RES     0x10  // 1 lx resolution, 120ms
#define BH1750_CONTINUOUS_H_RES2    0x11  // 0.5 lx resolution, 120ms
#define BH1750_CONTINUOUS_L_RES     0x13  // 4 lx resolution, 16ms
#define BH1750_ONE_TIME_H_RES       0x20  // 1 lx resolution, 120ms (one time)
#define BH1750_ONE_TIME_H_RES2      0x21  // 0.5 lx resolution, 120ms (one time)
#define BH1750_ONE_TIME_L_RES       0x23  // 4 lx resolution, 16ms (one time)

typedef struct BH1750FVI_sensor {
    sensor_base_t base;
    i2c_port_t i2c_port;
    uint8_t i2c_address;
    uint8_t mode;
} BH1750FVI_sensor_t;

typedef struct BH1750FVI_data {
    float lux;
    uint16_t raw_value;
} BH1750FVI_data_t;

void BH1750FVI_setup_i2c(sensor_base_t* self);
SensorStatus_t BH1750FVI_init(sensor_base_t* self, i2c_port_t i2c_port, uint8_t i2c_address, uint8_t mode);
SensorStatus_t BH1750FVI_read_data(sensor_base_t* self, void* data);
SensorStatus_t BH1750FVI_deinit(sensor_base_t* self);

#endif // BH1750FVI_SENSOR_H
