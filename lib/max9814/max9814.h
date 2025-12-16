#ifndef MAX9814_H
#define MAX9814_H

#include <stdint.h>
#include "esp_err.h"
#include "esp_adc/adc_continuous.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/**
 * @brief MAX9814 microphone amplifier library
 * 
 * This library provides thread-safe ADC sampling with DMA and RMS calculation
 * for the MAX9814 microphone amplifier module.
 */

// Default configuration values
#define MAX9814_DEFAULT_SAMPLES 1024
#define MAX9814_MAX_SAMPLES 4096
#define MAX9814_SAMPLE_RATE_HZ 20000
#define MAX9814_DMA_FRAME_SIZE 256

/**
 * @brief MAX9814 device structure
 */
typedef struct
{
    adc_continuous_handle_t adc_handle;     // ADC continuous handle (DMA)
    adc_channel_t channel;                  // ADC channel
    adc_atten_t attenuation;                // ADC attenuation
    uint8_t *dma_buffer;                    // DMA temporary buffer
    uint32_t *sample_buffer;                // Processed sample buffer
    uint32_t buffer_size;                   // Number of samples in buffer
    uint32_t sample_rate;                   // Sample rate in Hz
    uint32_t sample_index;                  // Current sample index
    SemaphoreHandle_t mutex;                // Mutex for thread safety
    bool initialized;                       // Initialization flag
    bool collecting;                        // Currently collecting flag
} max9814_t;

/**
 * @brief Configuration structure for MAX9814
 */
typedef struct
{
    adc_channel_t channel;                  // ADC channel to use
    adc_atten_t attenuation;                // ADC attenuation (voltage range)
    uint32_t buffer_size;                   // Number of samples to collect
    uint32_t sample_rate;                   // Sample rate in Hz (default: 20000)
} max9814_config_t;

/**
 * @brief Initialize the MAX9814 sensor
 * 
 * @param max9814 Pointer to MAX9814 structure
 * @param config Configuration parameters
 * @return esp_err_t ESP_OK on success
 */
esp_err_t max9814_init(max9814_t *max9814, const max9814_config_t *config);

/**
 * @brief Deinitialize and free resources
 * 
 * @param max9814 Pointer to MAX9814 structure
 * @return esp_err_t ESP_OK on success
 */
esp_err_t max9814_deinit(max9814_t *max9814);

/**
 * @brief Start continuous ADC sampling with DMA
 * 
 * Starts DMA-based ADC sampling in the background.
 * Thread-safe: protected by mutex.
 * 
 * @param max9814 Pointer to MAX9814 structure
 * @return esp_err_t ESP_OK on success
 */
esp_err_t max9814_start_sampling(max9814_t *max9814);

/**
 * @brief Stop continuous ADC sampling
 * 
 * Stops DMA-based ADC sampling.
 * Thread-safe: protected by mutex.
 * 
 * @param max9814 Pointer to MAX9814 structure
 * @return esp_err_t ESP_OK on success
 */
esp_err_t max9814_stop_sampling(max9814_t *max9814);

/**
 * @brief Collect samples from ADC (blocking)
 * 
 * Fills the internal buffer with ADC samples using DMA.
 * This is a blocking call that waits for the buffer to fill.
 * Thread-safe: protected by mutex.
 * 
 * @param max9814 Pointer to MAX9814 structure
 * @return esp_err_t ESP_OK on success
 */
esp_err_t max9814_collect_samples(max9814_t *max9814);

/**
 * @brief Read RMS value as percentage
 * 
 * Calculates the RMS (Root Mean Square) of the samples in the buffer
 * and returns it as a percentage (0-100%).
 * Thread-safe: protected by mutex.
 * 
 * @param max9814 Pointer to MAX9814 structure
 * @param rms_percent Pointer to store the RMS percentage value
 * @return esp_err_t ESP_OK on success
 */
esp_err_t read_max9814(max9814_t *max9814, float *rms_percent);

/**
 * @brief Read raw RMS value
 * 
 * Calculates and returns the raw RMS value in ADC units.
 * Thread-safe: protected by mutex.
 * 
 * @param max9814 Pointer to MAX9814 structure
 * @param rms_raw Pointer to store the raw RMS value
 * @return esp_err_t ESP_OK on success
 */
esp_err_t max9814_read_rms_raw(max9814_t *max9814, float *rms_raw);

/**
 * @brief Get the current buffer
 * 
 * Returns a copy of the current sample buffer.
 * Thread-safe: protected by mutex.
 * 
 * @param max9814 Pointer to MAX9814 structure
 * @param buffer Output buffer (must be pre-allocated)
 * @param buffer_size Size of the output buffer
 * @return esp_err_t ESP_OK on success
 */
esp_err_t max9814_get_buffer(max9814_t *max9814, uint32_t *buffer, uint32_t buffer_size);

#endif // MAX9814_H
