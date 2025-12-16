
#include "KY-037_sensor.h"
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "KY037";

void KY037_setup_adc(sensor_base_t *self)
{
    KY037_context_t *context = (KY037_context_t *)self->context;

    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    esp_err_t err = adc_oneshot_new_unit(&init_config, &(context->adc_handle));
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize ADC unit");
        return;
    }

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };
    err = adc_oneshot_config_channel(context->adc_handle, context->channel, &config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure ADC channel");
        return;
    }

    ESP_LOGI(TAG, "ADC initialized successfully");
}

SensorStatus_t KY037_init(sensor_base_t *self, adc_channel_t channel)
{
    if (self == NULL)
    {
        ESP_LOGE(TAG, "Invalid address for sensor");
        return SENSOR_ERROR;
    }

    if (self->context != NULL)
    {
        ESP_LOGE(TAG, "Failed to init KY-037. Sensor already initialized.");
        return SENSOR_ERROR;
    }

    self->context = malloc(sizeof(KY037_context_t));

    KY037_context_t *context = (KY037_context_t *)self->context;
    context->channel = channel;

    self->read_data = KY037_read_data;
    self->deinit = KY037_deinit;

    KY037_setup_adc(self);

    ESP_LOGI(TAG, "KY037 sensor initialized successfully on channel %d", channel);
    return SENSOR_OK;
}

SensorStatus_t KY037_read_data(sensor_base_t *self, void *data)
{
    if (self == NULL || data == NULL)
    {
        ESP_LOGE(TAG, "Invalid parameters");
        return SENSOR_ERROR;
    }

    KY037_context_t *context = (KY037_context_t *)self->context;
    uint32_t *adc_reading = (uint32_t *)data;

    esp_err_t err = adc_oneshot_read(context->adc_handle, context->channel, (int *)adc_reading);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read ADC data");
        return SENSOR_ERROR;
    }

    ESP_LOGI(TAG, "Sound level: %lu (raw ADC value)", *adc_reading);
    return SENSOR_OK;
}

SensorStatus_t KY037_deinit(sensor_base_t *self)
{
    if (self == NULL)
    {
        return SENSOR_ERROR;
    }

    KY037_context_t *context = (KY037_context_t *)self->context;
    if (context && context->adc_handle)
    {
        adc_oneshot_del_unit(context->adc_handle);
    }

    if (self->context != NULL)
    {
        free(self->context);
    }
    free(self);

    ESP_LOGI(TAG, "KY037 sensor deinitialized");
    return SENSOR_OK;
}
