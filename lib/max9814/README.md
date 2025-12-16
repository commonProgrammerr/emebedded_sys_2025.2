# MAX9814 Library

Thread-safe library for the MAX9814 microphone amplifier module with ADC sampling and RMS calculation for ESP32.

## Features

- Configurable ADC channel and buffer size
- Thread-safe operations using FreeRTOS mutexes
- RMS (Root Mean Square) calculation
- Returns RMS as percentage (0-100%)
- Blocking sample collection with configurable sample rate
- Raw RMS value access

## Hardware Setup

The MAX9814 is a microphone amplifier with automatic gain control (AGC). Connect it to an ADC-capable GPIO pin on your ESP32.

**Typical connections:**
- VCC → 3.3V
- GND → GND
- OUT → ADC GPIO (e.g., GPIO1-GPIO10 for ADC1)
- GAIN (optional) → Configure gain setting
- AR (optional) → Attack/Release ratio

## Usage Example

### Basic Usage

```c
#include "max9814.h"

void app_main(void)
{
    max9814_t mic_sensor;
    max9814_config_t config = {
        .channel = ADC_CHANNEL_0,           // GPIO1 for ESP32
        .attenuation = ADC_ATTEN_DB_11,     // 0-3.3V range
        .buffer_size = 1024                  // 1024 samples
    };

    // Initialize the sensor
    esp_err_t ret = max9814_init(&mic_sensor, &config);
    if (ret != ESP_OK) {
        ESP_LOGE("APP", "Failed to initialize MAX9814");
        return;
    }

    while (1) {
        // Collect samples (125us between samples = 8kHz sample rate)
        ret = max9814_collect_samples(&mic_sensor, 125);
        if (ret != ESP_OK) {
            ESP_LOGE("APP", "Failed to collect samples");
            continue;
        }

        // Read RMS as percentage
        float rms_percent;
        ret = read_max9814(&mic_sensor, &rms_percent);
        if (ret == ESP_OK) {
            ESP_LOGI("APP", "Noise level: %.2f%%", rms_percent);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // Cleanup
    max9814_deinit(&mic_sensor);
}
```

### Multi-threaded Usage

```c
max9814_t shared_mic;

void sampling_task(void *pvParameters)
{
    while (1) {
        // Collect samples at 8kHz
        max9814_collect_samples(&shared_mic, 125);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void reading_task(void *pvParameters)
{
    while (1) {
        float rms_percent;
        if (read_max9814(&shared_mic, &rms_percent) == ESP_OK) {
            ESP_LOGI("READER", "RMS: %.2f%%", rms_percent);
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void app_main(void)
{
    max9814_config_t config = {
        .channel = ADC_CHANNEL_0,
        .attenuation = ADC_ATTEN_DB_11,
        .buffer_size = 1024
    };

    max9814_init(&shared_mic, &config);

    xTaskCreate(sampling_task, "sampling", 4096, NULL, 5, NULL);
    xTaskCreate(reading_task, "reading", 4096, NULL, 5, NULL);
}
```

## API Reference

### Initialization

#### `max9814_init()`
```c
esp_err_t max9814_init(max9814_t *max9814, const max9814_config_t *config);
```
Initialize the MAX9814 sensor with specified configuration.

**Parameters:**
- `max9814`: Pointer to MAX9814 structure
- `config`: Configuration parameters (channel, attenuation, buffer size)

**Returns:** `ESP_OK` on success

---

#### `max9814_deinit()`
```c
esp_err_t max9814_deinit(max9814_t *max9814);
```
Deinitialize and free all resources.

---

### Data Collection

#### `max9814_collect_samples()`
```c
esp_err_t max9814_collect_samples(max9814_t *max9814, uint32_t delay_us);
```
Collect samples from ADC into internal buffer.

**Parameters:**
- `max9814`: Pointer to MAX9814 structure
- `delay_us`: Delay between samples in microseconds (0 for fastest rate)

**Example sample rates:**
- 125 µs → 8 kHz
- 100 µs → 10 kHz
- 50 µs → 20 kHz

**Thread-safe:** Yes

---

### Reading Data

#### `read_max9814()`
```c
esp_err_t read_max9814(max9814_t *max9814, float *rms_percent);
```
Calculate and return RMS value as percentage (0-100%).

**Parameters:**
- `max9814`: Pointer to MAX9814 structure
- `rms_percent`: Output pointer for RMS percentage

**Thread-safe:** Yes

---

#### `max9814_read_rms_raw()`
```c
esp_err_t max9814_read_rms_raw(max9814_t *max9814, float *rms_raw);
```
Get raw RMS value in ADC units (0-4095 for 12-bit ADC).

**Thread-safe:** Yes

---

#### `max9814_get_buffer()`
```c
esp_err_t max9814_get_buffer(max9814_t *max9814, uint32_t *buffer, uint32_t buffer_size);
```
Get a copy of the current sample buffer.

**Thread-safe:** Yes

---

## Configuration

### ADC Channels (ESP32)
Use ADC1 channels for WiFi compatibility:
- `ADC_CHANNEL_0` - GPIO36
- `ADC_CHANNEL_1` - GPIO37 (ESP32-S3)
- `ADC_CHANNEL_3` - GPIO39
- `ADC_CHANNEL_4` - GPIO32
- `ADC_CHANNEL_5` - GPIO33
- `ADC_CHANNEL_6` - GPIO34
- `ADC_CHANNEL_7` - GPIO35

### Attenuation Settings
- `ADC_ATTEN_DB_0` - 0-1.1V range
- `ADC_ATTEN_DB_2_5` - 0-1.5V range
- `ADC_ATTEN_DB_6` - 0-2.2V range
- `ADC_ATTEN_DB_11` - 0-3.3V range (recommended)

### Buffer Size
- Minimum: 1 sample
- Maximum: 4096 samples (defined by `MAX9814_MAX_SAMPLES`)
- Recommended: 512-2048 samples for good RMS accuracy

## RMS Calculation

The library calculates the RMS (Root Mean Square) value using the standard formula:

$$
RMS = \sqrt{\frac{1}{N}\sum_{i=1}^{N}(x_i - \bar{x})^2}
$$

Where:
- $N$ = number of samples
- $x_i$ = individual sample value
- $\bar{x}$ = mean of all samples

The percentage value is calculated as: `(RMS / 4095) * 100%`

## Thread Safety

All public functions are thread-safe using FreeRTOS mutexes. Multiple tasks can safely:
- Collect samples from one task
- Read RMS from another task
- Access the buffer from multiple tasks

The mutex ensures data consistency and prevents race conditions.

## Notes

- The MAX9814 output is AC-coupled, centered around VCC/2 (typically ~1.65V)
- For best results, use `ADC_ATTEN_DB_11` to capture the full voltage range
- Higher sample rates require shorter delays but may impact system performance
- The RMS calculation removes DC bias automatically by subtracting the mean

## License

This library is provided as-is for use with ESP-IDF projects.
