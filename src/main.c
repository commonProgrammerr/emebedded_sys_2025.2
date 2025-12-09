#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "bh1750fvi_sensor.h"
#include "dht11_sensor.h"
#include "KY-037_sensor.h"
#include "sensor_monitor.h"
#include "sensor_history.h"
#include "flash_buffer.h"

#define MIC_ADC_PIN 33
#define MIC_ADC_CHANEL ADC_CHANNEL_5
#define SPEAKER_PIN 34
#define RED_LED_PIN 26
#define YELLOW_LED_PIN 32
#define DHT11_PIN 23
#define BUTTON_PIN 22

#define DHT11_READ_INTERVAL_MS 5000
#define BH1750_READ_INTERVAL_MS 10000
#define KY037_READ_INTERVAL_MS 1000

flash_buffer_t *buffer = NULL;
full_sensor_read_t current_read = {0};

void save_dht11(sensor_base_t *sensor, void *data);
void save_ky037(sensor_base_t *sensor, void *data);
void save_bh1750(sensor_base_t *sensor, void *data);
void av_cal_monitor_timer_callback(TimerHandle_t xTimer);

void app_main(void)
{
    // Inicializa NVS
    esp_err_t ret = flash_buffer_system_init();
    if (ret != ESP_OK) {
        ESP_LOGE("main", "Falha ao inicializar NVS");
        return;
    }

    // Cria buffer (1440 amostras = 24h com 1 amostra por minuto)
    buffer = flash_buffer_init("sensors", sizeof(compact_sensor_read_t), 1440);
    if (!buffer) {
        ESP_LOGE("main", "Falha ao criar buffer");
        return;
    }


    // Histórico de 60 registros (1 amostra por segundo)
    init_history_system(60); 

    sensor_base_t bh1750 = {0}, dht11 = {0}, ky_037 = {0};

    bh1750fvi_init(&bh1750, SDA_IO, SCL_IO, BH1750_I2C_ADDR_LOW, BH1750_CONT_H_RES);
    dht11_init(&dht11, DHT11_PIN, 5000);  // Aumentar timeout para 5000ms
    KY037_init(&ky_037, MIC_ADC_CHANEL);

    sensor_monitor_t *th_monitor = new_sensor_monitor(
        &dht11,
        DHT11_READ_INTERVAL_MS,
        sizeof(dht11_t),
        "temp&humidity_monitor",
        save_dht11);

    sensor_monitor_t *noise_monitor = new_sensor_monitor(
        &ky_037,
        KY037_READ_INTERVAL_MS,
        sizeof(float),
        "noise_monitor",
        save_ky037);

    sensor_monitor_t *light_monitor = new_sensor_monitor(
        &bh1750,
        BH1750_READ_INTERVAL_MS,
        sizeof(float),
        "light_monitor",
        save_bh1750);

    if (th_monitor) start_sensor_monitoring(th_monitor);
    if (noise_monitor) start_sensor_monitoring(noise_monitor);
    if (light_monitor) start_sensor_monitoring(light_monitor);
    
    ESP_LOGI("main", "Sistema iniciado. Monitorando sensores...");
    
    // Cria e inicia o timer de média móvel
    TimerHandle_t av_timer = xTimerCreate(
        "av_calculation_timer",
        pdMS_TO_TICKS(60000),  // 60 segundos
        pdTRUE,                // Auto-reload (repetir)
        NULL,                  // Timer ID
        av_cal_monitor_timer_callback
    );

    if (av_timer != NULL) {
        if (xTimerStart(av_timer, 0) == pdPASS) {
            ESP_LOGI("main", "Timer de média móvel iniciado (60s)");
        } else {
            ESP_LOGE("main", "Falha ao iniciar timer");
        }
    } else {
        ESP_LOGE("main", "Falha ao criar timer");
    }

    for (;;)
        vTaskDelay(portMAX_DELAY);
}

void save_dht11(sensor_base_t *sensor, void *data)
{
    dht11_t *dht_data = (dht11_t *)data;
    current_read.temperature = dht_data->temperature;
    current_read.humidity = dht_data->humidity;
    save_sensor_read(&current_read);   
}

void save_ky037(sensor_base_t *sensor, void *data)
{
    uint16_t *noise_level = (uint16_t *)data;
    current_read.noise_level = *noise_level;
    save_sensor_read(&current_read);
}

void save_bh1750(sensor_base_t *sensor, void *data)
{
    float *lux = (float *)data;
    current_read.lux = *lux;
    save_sensor_read(&current_read);
}

void av_cal_monitor_timer_callback(TimerHandle_t xTimer) {
    compact_sensor_read_t compact_read;
    if(get_moving_average(&compact_read)) {
        ESP_LOGI("AV_CAL", "Média móvel - Temp: %.2f °C, Hum: %.2f %%, Lux: %.2f lx, Noise: %.2f",
                 RAW_TO_TEMP(compact_read.temperature),
                 RAW_TO_HUMID(compact_read.humidity),
                 RAW_TO_LUX(compact_read.lux),
                 RAW_TO_NOISE(compact_read.noise_level));
        flash_buffer_write(buffer, &compact_read); // save to flash buffer
    }
}
