#ifndef KY_037_SENSOR_H
#define KY_037_SENSOR_H
#include <stdint.h>
#include "sensor_base.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "max9814.h"

typedef struct noise_sensor_context
{
    max9814_t mic_sensor;
    max9814_config_t config;
} noise_sensor_context_t;

SensorStatus_t noise_sensor_init(sensor_base_t *self, adc_channel_t channel);
SensorStatus_t noise_sensor_read_data(sensor_base_t *self, void *data);
SensorStatus_t noise_sensor_deinit(sensor_base_t *self);

#endif // KY_037_SENSOR_H