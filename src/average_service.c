#include "average_service.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "AVG_SERVICE";

// Buffer circular e variáveis de controle
static float samples[SAMPLE_BUFFER_SIZE];
static int sample_index = 0;
static bool buffer_full = false;

// Controle de tempo (1 minuto)
static int64_t last_calc_time = 0;
#define CALC_INTERVAL_US 60000000 // 60 segundos

// Função Auxiliar: Calcular Média
float calculate_moving_average() {
    float sum = 0;
    int count = buffer_full ? SAMPLE_BUFFER_SIZE : sample_index;
    
    if (count == 0) return 0.0f;

    for (int i = 0; i < count; i++) {
        sum += samples[i];
    }
    return sum / count;
}

void average_task(void *pvParameters) {
    QueueHandle_t inputQueue = (QueueHandle_t)pvParameters;
    sensor_data_t receivedData;
    
    last_calc_time = esp_timer_get_time();

    while (1) {
        // 1. Consome a fila (Espera dados chegarem)
        if (xQueueReceive(inputQueue, &receivedData, pdMS_TO_TICKS(100)) == pdTRUE) {
            
            // Adiciona ao Buffer
            samples[sample_index] = receivedData.sensor_value;
            sample_index++;
            
            if (sample_index >= SAMPLE_BUFFER_SIZE) {
                sample_index = 0;
                buffer_full = true;
            }
            // Log apenas para debug
            // ESP_LOGI(TAG, "Dado recebido: %.2f", receivedData.sensor_value);
        }

        // 2. A cada 1 minuto, calcula a média (Só em RAM)
        if ((esp_timer_get_time() - last_calc_time) > CALC_INTERVAL_US) {
            float avg = calculate_moving_average();
            
            // Apenas mostra no log (O mentor vai salvar isso na Task #37 depois)
            ESP_LOGI(TAG, "Media Movel (1min): %.2f", avg);
            
            last_calc_time = esp_timer_get_time();
        }
    }
}