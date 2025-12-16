#include "max9814.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"

static const char *TAG = "MAX9814";

// ADC maximum value (12-bit ADC on ESP32)
#define ADC_MAX_VALUE 4095

// DMA buffer configuration
#define DMA_BUF_SIZE 1024
#define DMA_BUF_COUNT 2

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
    max9814->sample_rate = (config->sample_rate > 0) ? config->sample_rate : MAX9814_SAMPLE_RATE_HZ;
    max9814->collecting = false;

    // Allocate sample buffer
    max9814->sample_buffer = (uint32_t *)malloc(config->buffer_size * sizeof(uint32_t));
    if (max9814->sample_buffer == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate sample buffer");
        return ESP_ERR_NO_MEM;
    }

    // Allocate DMA buffer
    max9814->dma_buffer = (uint8_t *)malloc(DMA_BUF_SIZE);
    if (max9814->dma_buffer == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate DMA buffer");
        free(max9814->sample_buffer);
        return ESP_ERR_NO_MEM;
    }

    // Initialize buffers with zeros
    memset(max9814->sample_buffer, 0, config->buffer_size * sizeof(uint32_t));
    memset(max9814->dma_buffer, 0, DMA_BUF_SIZE);

    // Create mutex for thread safety
    max9814->mutex = xSemaphoreCreateMutex();
    if (max9814->mutex == NULL)
    {
        ESP_LOGE(TAG, "Failed to create mutex");
        free(max9814->dma_buffer);
        free(max9814->sample_buffer);
        return ESP_ERR_NO_MEM;
    }

    // Configure ADC continuous mode with DMA
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = DMA_BUF_SIZE * DMA_BUF_COUNT,
        .conv_frame_size = DMA_BUF_SIZE,
    };

    esp_err_t ret = adc_continuous_new_handle(&adc_config, &max9814->adc_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create ADC continuous handle: %s", esp_err_to_name(ret));
        vSemaphoreDelete(max9814->mutex);
        free(max9814->dma_buffer);
        free(max9814->sample_buffer);
        return ret;
    }

    // Configure ADC channel and pattern
    adc_digi_pattern_config_t adc_pattern = {
        .atten = config->attenuation,
        .channel = config->channel,
        .unit = ADC_UNIT_1,
        .bit_width = SOC_ADC_DIGI_MAX_BITWIDTH,
    };

    adc_continuous_config_t dig_cfg = {
        .pattern_num = 1,
        .adc_pattern = &adc_pattern,
        .sample_freq_hz = max9814->sample_rate,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1,
    };

    ret = adc_continuous_config(max9814->adc_handle, &dig_cfg);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure ADC: %s", esp_err_to_name(ret));
        adc_continuous_deinit(max9814->adc_handle);
        vSemaphoreDelete(max9814->mutex);
        free(max9814->dma_buffer);
        free(max9814->sample_buffer);
        return ret;
    }

    max9814->initialized = true;
    ESP_LOGI(TAG, "MAX9814 initialized with DMA (channel: %d, buffer: %lu samples, rate: %lu Hz)",
             config->channel, config->buffer_size, max9814->sample_rate);

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

    // Stop sampling if active
    if (max9814->collecting)
    {
        adc_continuous_stop(max9814->adc_handle);
    }

    // Take mutex before cleanup
    if (xSemaphoreTake(max9814->mutex, portMAX_DELAY) == pdTRUE)
    {
        // Deinitialize ADC
        if (max9814->adc_handle != NULL)
        {
            adc_continuous_deinit(max9814->adc_handle);
        }

        // Free buffers
        if (max9814->sample_buffer != NULL)
        {
            free(max9814->sample_buffer);
            max9814->sample_buffer = NULL;
        }

        if (max9814->dma_buffer != NULL)
        {
            free(max9814->dma_buffer);
            max9814->dma_buffer = NULL;
        }

        max9814->initialized = false;
        max9814->collecting = false;
        xSemaphoreGive(max9814->mutex);
    }

    // Delete mutex
    vSemaphoreDelete(max9814->mutex);
    max9814->mutex = NULL;

    ESP_LOGI(TAG, "MAX9814 deinitialized");
    return ESP_OK;
}

/**
 * @brief Start continuous ADC sampling
 */
esp_err_t max9814_start_sampling(max9814_t *max9814)
{
    if (max9814 == NULL || !max9814->initialized)
    {
        ESP_LOGE(TAG, "MAX9814 not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Take mutex for thread safety
    if (xSemaphoreTake(max9814->mutex, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to take mutex");
        return ESP_ERR_TIMEOUT;
    }

    if (max9814->collecting)
    {
        ESP_LOGW(TAG, "Already collecting samples");
        xSemaphoreGive(max9814->mutex);
        return ESP_OK;
    }

    esp_err_t ret = adc_continuous_start(max9814->adc_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start ADC: %s", esp_err_to_name(ret));
        xSemaphoreGive(max9814->mutex);
        return ret;
    }

    max9814->collecting = true;
    xSemaphoreGive(max9814->mutex);

    ESP_LOGI(TAG, "Started continuous sampling at %lu Hz", max9814->sample_rate);
    return ESP_OK;
}

/**
 * @brief Stop continuous ADC sampling
 */
esp_err_t max9814_stop_sampling(max9814_t *max9814)
{
    if (max9814 == NULL || !max9814->initialized)
    {
        ESP_LOGE(TAG, "MAX9814 not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Take mutex for thread safety
    if (xSemaphoreTake(max9814->mutex, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to take mutex");
        return ESP_ERR_TIMEOUT;
    }

    if (!max9814->collecting)
    {
        xSemaphoreGive(max9814->mutex);
        return ESP_OK;
    }

    esp_err_t ret = adc_continuous_stop(max9814->adc_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to stop ADC: %s", esp_err_to_name(ret));
        xSemaphoreGive(max9814->mutex);
        return ret;
    }

    max9814->collecting = false;
    xSemaphoreGive(max9814->mutex);

    ESP_LOGI(TAG, "Stopped continuous sampling");
    return ESP_OK;
}

/**
 * @brief Collect samples from ADC using DMA
 */
esp_err_t max9814_collect_samples(max9814_t *max9814)
{
    if (max9814 == NULL || !max9814->initialized)
    {
        ESP_LOGE(TAG, "MAX9814 not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Take mutex for thread safety
    if (xSemaphoreTake(max9814->mutex, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to take mutex");
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = ESP_OK;
    uint32_t sample_count = 0;
    bool was_collecting = max9814->collecting;

    // Start ADC if not already running
    if (!max9814->collecting)
    {
        ret = adc_continuous_start(max9814->adc_handle);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to start ADC: %s", esp_err_to_name(ret));
            xSemaphoreGive(max9814->mutex);
            return ret;
        }
        max9814->collecting = true;
    }

    // Read samples until buffer is full
    while (sample_count < max9814->buffer_size)
    {
        uint32_t bytes_read = 0;
        ret = adc_continuous_read(max9814->adc_handle, max9814->dma_buffer, 
                                  DMA_BUF_SIZE, &bytes_read, portMAX_DELAY);
        
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "ADC read failed: %s", esp_err_to_name(ret));
            break;
        }

        // Process DMA buffer - extract ADC values
        for (uint32_t i = 0; i < bytes_read && sample_count < max9814->buffer_size; i += SOC_ADC_DIGI_RESULT_BYTES)
        {
            adc_digi_output_data_t *p = (adc_digi_output_data_t *)&max9814->dma_buffer[i];
            
            // Extract 12-bit ADC value from DMA format
            uint32_t adc_value = p->type1.data;
            
            max9814->sample_buffer[sample_count++] = adc_value;
        }
    }

    max9814->sample_index = sample_count;

    // Stop ADC if we started it
    if (!was_collecting && max9814->collecting)
    {
        adc_continuous_stop(max9814->adc_handle);
        max9814->collecting = false;
    }

    xSemaphoreGive(max9814->mutex);

    ESP_LOGD(TAG, "Collected %lu samples via DMA", sample_count);
    return ret;
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
