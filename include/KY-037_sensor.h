#ifndef KY_037_SENSOR_H
#define KY_037_SENSOR_H
#include <stdint.h>
#include "sensor_base.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

typedef struct KY037_sensor {
    sensor_base_t base;
    adc_channel_t channel;
    adc_oneshot_unit_handle_t adc_handle;
} KY037_sensor_t;

void KY037_setup_adc(sensor_base_t* self);
SensorStatus_t KY037_init(sensor_base_t* self, adc_channel_t channel);
SensorStatus_t KY037_read_data(sensor_base_t* self, void* data);
SensorStatus_t KY037_deinit(sensor_base_t* self);

#endif // KY_037_SENSOR_H