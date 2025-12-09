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

// --- NOVO INCLUDE DA TASK #32 ---
#include "button_driver.h" 

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

// Bits de Notificação para os Eventos do Botão
#define EVT_BTN_CLICKED  0x01
#define EVT_BTN_LONG     0x02

flash_buffer_t *buffer = NULL;
full_sensor_read_t current_read = {0};

// Variáveis para o sistema de botões
TaskHandle_t xMainTaskHandle = NULL;
Button_t btn_nav;

void save_dht11(sensor_base_t *sensor, void *data);
void save_ky037(sensor_base_t *sensor, void *data);
void save_bh1750(sensor_base_t *sensor, void *data);
void av_cal_monitor_timer_callback(TimerHandle_t xTimer);

// --- CALLBACK DO BOTÃO (Task #32) ---
// O driver chama isso, e isso avisa a main task via Notify
void my_button_callback(int pin, button_event_t event) {
    if (xMainTaskHandle != NULL) {
        if (event == BUTTON_PRESS_LONG) {
            xTaskNotify(xMainTaskHandle, EVT_BTN_LONG, eSetBits);
        } else {
            // Curto ou Normal tratamos como clique simples
            xTaskNotify(xMainTaskHandle, EVT_BTN_CLICKED, eSetBits);
        }
    }
}

void app_main(void)
{
    // 1. Captura o Handle da Task Main (Para receber notificações)
    xMainTaskHandle = xTaskGetCurrentTaskHandle();

    // 2. Inicializa NVS
    esp_err_t ret = flash_buffer_system_init();
    if (ret != ESP_OK) {
        ESP_LOGE("main", "Falha ao inicializar NVS");
        return;
    }

    // 3. Inicializa Buffer e Histórico
    buffer = flash_buffer_init("sensors", sizeof(compact_sensor_read_t), 1440);
    if (!buffer) {
        ESP_LOGE("main", "Falha ao criar buffer");
        return;
    }
    init_history_system(60); 

    // 4. Inicializa o Botão com o novo Driver (Task #32)
    // Passamos o pino definido e a função de callback criada acima
    button_init(&btn_nav, (gpio_num_t)BUTTON_PIN, my_button_callback);

    // 5. Inicializa Sensores
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
    
    // 6. Configura Timer de Média
    TimerHandle_t av_timer = xTimerCreate(
        "av_calculation_timer", pdMS_TO_TICKS(60000), pdTRUE, NULL, av_cal_monitor_timer_callback
    );

    if (av_timer != NULL) xTimerStart(av_timer, 0);

    // 7. Loop Principal Atualizado (Processamento de Botão e Notificações)
    uint32_t notification_value = 0;
    
    for (;;)
    {
        // A. Processa o driver do botão (Polling)
        button_process(&btn_nav);

        // B. Verifica se o callback do botão mandou algum sinal
        // Timeout 0 para não travar o loop do botão
        if (xTaskNotifyWait(0, 0xFFFFFFFF, &notification_value, 0) == pdTRUE) {
            
            if (notification_value & EVT_BTN_LONG) {
                ESP_LOGI("MAIN", "EVENTO: Botao Long Press Detectado!");
                // Adicione lógica de reset ou menu aqui
            }
            
            if (notification_value & EVT_BTN_CLICKED) {
                ESP_LOGI("MAIN", "EVENTO: Botao Clique Simples Detectado!");
                // Adicione lógica de navegação aqui
            }
        }

        // Delay curto para definir a taxa de amostragem do botão (100Hz = 10ms)
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// --- Funções de Callback dos Sensores (Mantidas iguais) ---

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
        flash_buffer_write(buffer, &compact_read);
    }
}