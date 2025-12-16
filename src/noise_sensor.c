
#include "noise_sensor.h"
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "NOISE";

SensorStatus_t noise_sensor_init(sensor_base_t *self, adc_channel_t channel)
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

    self->context = malloc(sizeof(noise_sensor_context_t));

    noise_sensor_context_t *context = (noise_sensor_context_t *)self->context;

    self->read_data = noise_sensor_read_data;
    self->deinit = noise_sensor_deinit;

    context->config = ((max9814_config_t) {
        .channel = channel,          
        .attenuation = ADC_ATTEN_DB_11,     // 0-3.3V range
        .buffer_size = 1024                 // 1024 samples
    });

    // Initialize the sensor
    esp_err_t ret = max9814_init(&context->mic_sensor, &context->config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize MAX9814");
        return SENSOR_ERROR;
    }

    ESP_LOGI(TAG, "Noise sensor initialized successfully on channel %d", channel);
    return SENSOR_OK;
}

SensorStatus_t noise_sensor_read_data(sensor_base_t *self, void *data)
{
    if (self == NULL || data == NULL)
    {
        ESP_LOGE(TAG, "Invalid parameters");
        return SENSOR_ERROR;
    }

    noise_sensor_context_t *context = (noise_sensor_context_t *)self->context;
    
    // Collect samples (125us between samples = 8kHz sample rate)
    esp_err_t ret = max9814_collect_samples(&context->mic_sensor, 125);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to collect samples: %s", esp_err_to_name(ret));
        return SENSOR_ERROR;
    }
    
    // Read RMS as percentage
    float rms_percent;
    ret = read_max9814(&context->mic_sensor, &rms_percent);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read RMS value: %s", esp_err_to_name(ret));
        return SENSOR_ERROR;
    }
    
    // Copy result to output parameter
    *(float *)data = rms_percent;
    
    ESP_LOGI(TAG, "Read level: %.2f%%", rms_percent);
    return SENSOR_OK;

}

SensorStatus_t noise_sensor_deinit(sensor_base_t *self)
{
    if (self == NULL)
    {
        return SENSOR_ERROR;
    }

    noise_sensor_context_t *context = (noise_sensor_context_t *)self->context;
    if (context && context->mic_sensor.initialized)
    {
        max9814_deinit(&context->mic_sensor);
    }

    if (self->context != NULL)
    {
        free(self->context);
    }
    free(self);

    ESP_LOGI(TAG, "Noise sensor deinitialized");
    return SENSOR_OK;
}
