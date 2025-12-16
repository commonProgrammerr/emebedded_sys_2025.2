#include "max9814.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MAX9814";

// ADC maximum value (12-bit ADC on ESP32)
#define ADC_MAX_VALUE 4095

/**
 * @brief Initialize the MAX9814 sensor
 */
esp_err_t max9814_init(max9814_t *max9814, const max9814_config_t *config)
{
    if (max9814 == NULL || config == NULL)
    {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }

    if (config->buffer_size == 0 || config->buffer_size > MAX9814_MAX_SAMPLES)
    {
        ESP_LOGE(TAG, "Invalid buffer size: %lu (max: %d)", config->buffer_size, MAX9814_MAX_SAMPLES);
        return ESP_ERR_INVALID_ARG;
    }

    // Initialize structure
    memset(max9814, 0, sizeof(max9814_t));
    max9814->channel = config->channel;
    max9814->attenuation = config->attenuation;
    max9814->buffer_size = config->buffer_size;

    // Allocate sample buffer
    max9814->sample_buffer = (uint32_t *)malloc(config->buffer_size * sizeof(uint32_t));
    if (max9814->sample_buffer == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate sample buffer");
        return ESP_ERR_NO_MEM;
    }

    // Initialize buffer with zeros
    memset(max9814->sample_buffer, 0, config->buffer_size * sizeof(uint32_t));

    // Create mutex for thread safety
    max9814->mutex = xSemaphoreCreateMutex();
    if (max9814->mutex == NULL)
    {
        ESP_LOGE(TAG, "Failed to create mutex");
        free(max9814->sample_buffer);
        return ESP_ERR_NO_MEM;
    }

    // Configure ADC oneshot mode
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };

    esp_err_t ret = adc_oneshot_new_unit(&init_config, &max9814->adc_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize ADC unit: %s", esp_err_to_name(ret));
        vSemaphoreDelete(max9814->mutex);
        free(max9814->sample_buffer);
        return ret;
    }

    // Configure ADC channel
    adc_oneshot_chan_cfg_t chan_config = {
        .atten = config->attenuation,
        .bitwidth = ADC_BITWIDTH_12,
    };

    ret = adc_oneshot_config_channel(max9814->adc_handle, config->channel, &chan_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure ADC channel: %s", esp_err_to_name(ret));
        adc_oneshot_del_unit(max9814->adc_handle);
        vSemaphoreDelete(max9814->mutex);
        free(max9814->sample_buffer);
        return ret;
    }

    max9814->initialized = true;
    ESP_LOGI(TAG, "MAX9814 initialized (channel: %d, buffer: %lu samples)",
             config->channel, config->buffer_size);

    return ESP_OK;
}

/**
 * @brief Deinitialize and free resources
 */
esp_err_t max9814_deinit(max9814_t *max9814)
{
    if (max9814 == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!max9814->initialized)
    {
        return ESP_OK;
    }

    // Take mutex before cleanup
    if (xSemaphoreTake(max9814->mutex, portMAX_DELAY) == pdTRUE)
    {
        // Delete ADC unit
        if (max9814->adc_handle != NULL)
        {
            adc_oneshot_del_unit(max9814->adc_handle);
        }

        // Free buffer
        if (max9814->sample_buffer != NULL)
        {
            free(max9814->sample_buffer);
            max9814->sample_buffer = NULL;
        }

        max9814->initialized = false;
        xSemaphoreGive(max9814->mutex);
    }

    // Delete mutex
    vSemaphoreDelete(max9814->mutex);
    max9814->mutex = NULL;

    ESP_LOGI(TAG, "MAX9814 deinitialized");
    return ESP_OK;
}

/**
 * @brief Collect samples from ADC
 */
esp_err_t max9814_collect_samples(max9814_t *max9814, uint32_t delay_us)
{
    if (max9814 == NULL || !max9814->initialized)
    {
        ESP_LOGE(TAG, "MAX9814 not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Take mutex for thread safety
    if (xSemaphoreTake(max9814->mutex, pdMS_TO_TICKS(1000)) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to take mutex");
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = ESP_OK;
    int adc_reading;

    // Collect samples
    for (uint32_t i = 0; i < max9814->buffer_size; i++)
    {
        ret = adc_oneshot_read(max9814->adc_handle, max9814->channel, &adc_reading);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "ADC read failed at sample %lu: %s", i, esp_err_to_name(ret));
            xSemaphoreGive(max9814->mutex);
            return ret;
        }

        max9814->sample_buffer[i] = (uint32_t)adc_reading;

        // Delay between samples if specified
        if (delay_us > 0)
        {
            esp_rom_delay_us(delay_us);
        }
        
        // Yield periodically to prevent watchdog
        if (i % 128 == 0 && i > 0)
        {
            taskYIELD();
        }
    }

    max9814->sample_index = max9814->buffer_size;
    xSemaphoreGive(max9814->mutex);

    ESP_LOGD(TAG, "Collected %lu samples", max9814->buffer_size);
    return ESP_OK;
}

/**
 * @brief Calculate RMS from buffer
 * 
 * Internal helper function - assumes mutex is already taken
 */
static float calculate_rms(max9814_t *max9814)
{
    if (max9814->sample_index == 0)
    {
        return 0.0f;
    }

    // Calculate mean
    uint64_t sum = 0;
    for (uint32_t i = 0; i < max9814->sample_index; i++)
    {
        sum += max9814->sample_buffer[i];
    }
    float mean = (float)sum / max9814->sample_index;

    // Calculate sum of squared differences from mean
    float sum_squared = 0.0f;
    for (uint32_t i = 0; i < max9814->sample_index; i++)
    {
        float diff = (float)max9814->sample_buffer[i] - mean;
        sum_squared += diff * diff;
    }

    // Calculate RMS
    float rms = sqrtf(sum_squared / max9814->sample_index);
    return rms;
}

/**
 * @brief Read RMS value as percentage
 */
esp_err_t read_max9814(max9814_t *max9814, float *rms_percent)
{
    if (max9814 == NULL || !max9814->initialized || rms_percent == NULL)
    {
        ESP_LOGE(TAG, "Invalid parameters or MAX9814 not initialized");
        return ESP_ERR_INVALID_ARG;
    }

    // Take mutex for thread safety
    if (xSemaphoreTake(max9814->mutex, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to take mutex");
        return ESP_ERR_TIMEOUT;
    }

    if (max9814->sample_index == 0)
    {
        ESP_LOGW(TAG, "No samples in buffer");
        *rms_percent = 0.0f;
        xSemaphoreGive(max9814->mutex);
        return ESP_OK;
    }

    // Calculate RMS
    float rms_raw = calculate_rms(max9814);

    // Convert to percentage (0-100%)
    // RMS can theoretically be up to ADC_MAX_VALUE, but in practice
    // it will be lower due to AC-coupled signal
    *rms_percent = (rms_raw / ADC_MAX_VALUE) * 100.0f;

    // Clamp to 0-100%
    if (*rms_percent > 100.0f)
    {
        *rms_percent = 100.0f;
    }
    else if (*rms_percent < 0.0f)
    {
        *rms_percent = 0.0f;
    }

    xSemaphoreGive(max9814->mutex);

    ESP_LOGD(TAG, "RMS: %.2f%% (raw: %.2f)", *rms_percent, rms_raw);
    return ESP_OK;
}

/**
 * @brief Read raw RMS value
 */
esp_err_t max9814_read_rms_raw(max9814_t *max9814, float *rms_raw)
{
    if (max9814 == NULL || !max9814->initialized || rms_raw == NULL)
    {
        ESP_LOGE(TAG, "Invalid parameters or MAX9814 not initialized");
        return ESP_ERR_INVALID_ARG;
    }

    // Take mutex for thread safety
    if (xSemaphoreTake(max9814->mutex, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to take mutex");
        return ESP_ERR_TIMEOUT;
    }

    if (max9814->sample_index == 0)
    {
        ESP_LOGW(TAG, "No samples in buffer");
        *rms_raw = 0.0f;
        xSemaphoreGive(max9814->mutex);
        return ESP_OK;
    }

    *rms_raw = calculate_rms(max9814);
    xSemaphoreGive(max9814->mutex);

    return ESP_OK;
}

/**
 * @brief Get the current buffer
 */
esp_err_t max9814_get_buffer(max9814_t *max9814, uint32_t *buffer, uint32_t buffer_size)
{
    if (max9814 == NULL || !max9814->initialized || buffer == NULL)
    {
        ESP_LOGE(TAG, "Invalid parameters or MAX9814 not initialized");
        return ESP_ERR_INVALID_ARG;
    }

    if (buffer_size < max9814->buffer_size)
    {
        ESP_LOGE(TAG, "Output buffer too small (%lu < %lu)", buffer_size, max9814->buffer_size);
        return ESP_ERR_INVALID_SIZE;
    }

    // Take mutex for thread safety
    if (xSemaphoreTake(max9814->mutex, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to take mutex");
        return ESP_ERR_TIMEOUT;
    }

    // Copy buffer
    memcpy(buffer, max9814->sample_buffer, max9814->sample_index * sizeof(uint32_t));
    xSemaphoreGive(max9814->mutex);

    return ESP_OK;
}
