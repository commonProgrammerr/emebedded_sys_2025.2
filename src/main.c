#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "bh1750fvi_sensor.h"
#include "dht11_sensor.h"
#include "KY-037_sensor.h"
#include "sensor_monitor.h"
#include "sensor_history.h"
#include "flash_buffer.h"
#include "flash_record.h"
#include "esp_timer.h"
#include "button_driver.h"
#include "uart_json_handler.h" 

#define MIC_ADC_PIN 33
#define MIC_ADC_CHANEL ADC_CHANNEL_5
#define SPEAKER_PIN 34
#define RED_LED_PIN 26
#define YELLOW_LED_PIN 32
#define DHT11_PIN 23
#define BUTTON_PIN 22

#define DHT11_READ_INTERVAL_MS 2000
#define BH1750_READ_INTERVAL_MS 10000
#define KY037_READ_INTERVAL_MS 1000

// Bits para eventos do botão via xTaskNotify
#define EVT_BTN_CLICKED  0x01
#define EVT_BTN_LONG     0x02

flash_buffer_t *buffer = NULL;

// Inicia a struct com valores minimos para evitar erro no calculo de média móvel
full_sensor_read_t current_read = {
    .temperature = -50.0f, 
    .humidity = 20.0f,     
    .lux = 0.0f,
    .noise_level = 0
};

// Sistema de botões com notificação assíncrona
TaskHandle_t xMainTaskHandle = NULL;
uint32_t btn_notification_value = 0;
Button_t btn_nav;

// Callbacks de salvamento de dados dos sensores
void save_dht11(sensor_base_t *sensor, void *data);
void save_ky037(sensor_base_t *sensor, void *data);
void save_bh1750(sensor_base_t *sensor, void *data);

// Timer callback para cálculo de média móvel
void av_cal_monitor_timer_callback(TimerHandle_t xTimer);

// Callback do botão
void button_callback(int pin, button_event_t event);

void app_main(void)
{
    xMainTaskHandle = xTaskGetCurrentTaskHandle();
    
    esp_err_t ret = flash_buffer_system_init();
    if (ret != ESP_OK) {
        ESP_LOGE("main", "Falha ao inicializar NVS");
        return;
    }

    // Cria buffer (1440 amostras = 24h com 1 amostra por minuto)
    buffer = flash_buffer_init("sensors", sizeof(flash_record_t), 1440);
    if (!buffer) {
        ESP_LOGE("main", "Falha ao criar buffer");
        return;
    }
    
    // Inicializa sistema de histórico em RAM
    init_history_system(60); 

    // Inicializa botão com interrupção GPIO e callback assíncrono
    button_init(&btn_nav, (gpio_num_t)BUTTON_PIN, button_callback, xMainTaskHandle);

    // Teste: aguarda 5s por evento do botão, se detectar faz reboot
    if (xTaskNotifyWait(0, UINT32_MAX, &btn_notification_value, pdMS_TO_TICKS(5000)) == pdTRUE) {
        
        if (btn_notification_value & EVT_BTN_LONG) {
            uart_json_dump_flash_and_restart(buffer);
        }
        
        if (btn_notification_value & EVT_BTN_CLICKED) {
            ESP_LOGI("BUTTON_TEST", ">>> EVENTO DETECTADO: Click Simples <<<");
        }
        
        ESP_LOGW("BUTTON_TEST", "Evento detectado! Reiniciando em 1 segundo...");
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGW("BUTTON_TEST", "REBOOTING NOW!");
        esp_restart();
    }

    sensor_base_t bh1750 = {0}, dht11 = {0}, ky_037 = {0};

    bh1750fvi_init(&bh1750, SDA_IO, SCL_IO, BH1750_I2C_ADDR_LOW, BH1750_CONT_H_RES);
    dht11_init(&dht11, DHT11_PIN, 5000); 
    KY037_init(&ky_037, MIC_ADC_CHANEL);

    sensor_monitor_t *th_monitor = new_sensor_monitor(
        &dht11, DHT11_READ_INTERVAL_MS, sizeof(dht11_t), "temp&humidity_monitor", save_dht11);

    sensor_monitor_t *noise_monitor = new_sensor_monitor(
        &ky_037, KY037_READ_INTERVAL_MS, sizeof(float), "noise_monitor", save_ky037);

    sensor_monitor_t *light_monitor = new_sensor_monitor(
        &bh1750, BH1750_READ_INTERVAL_MS, sizeof(float), "light_monitor", save_bh1750);

    if (th_monitor) start_sensor_monitoring(th_monitor);
    if (noise_monitor) start_sensor_monitoring(noise_monitor);
    if (light_monitor) start_sensor_monitoring(light_monitor);
    
    ESP_LOGI("main", "Sistema iniciado. Monitorando sensores e botoes...");
    if (th_monitor) start_sensor_monitoring(th_monitor);
    if (noise_monitor) start_sensor_monitoring(noise_monitor);
    if (light_monitor) start_sensor_monitoring(light_monitor);
    
    ESP_LOGI("main", "Sistema iniciado. Monitorando sensores e botoes...");
    
    // Timer para cálculo de média móvel a cada 60s
    TimerHandle_t av_timer = xTimerCreate(
        "av_calculation_timer", pdMS_TO_TICKS(60000), pdTRUE, NULL, av_cal_monitor_timer_callback
    );

    if (av_timer != NULL) xTimerStart(av_timer, 0);
    
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
        ESP_LOGI("AV_CAL", "Media movel - Temp: %.2f C, Hum: %.2f %%, Lux: %.2f lx, Noise: %.2f",
                 RAW_TO_TEMP(compact_read.temperature),
                 RAW_TO_HUMID(compact_read.humidity),
                 RAW_TO_LUX(compact_read.lux),
                 RAW_TO_NOISE(compact_read.noise_level));
        // Monta registro com timestamp e grava na flash
        flash_record_t frec = {0};
        frec.timestamp = (uint32_t)(esp_timer_get_time() / 1000000ULL);
        frec.compact = compact_read;
        flash_buffer_write(buffer, &frec);
    }
}

void button_callback(int pin, button_event_t event)
{
    uint32_t notify_value = 0;
    
    if (event == BUTTON_PRESS_LONG) {
        notify_value |= EVT_BTN_LONG;
    } else if (event == BUTTON_PRESS_SHORT) {
        notify_value |= EVT_BTN_CLICKED;
    }
    
    xTaskNotify(xMainTaskHandle, notify_value, eSetBits);
}
