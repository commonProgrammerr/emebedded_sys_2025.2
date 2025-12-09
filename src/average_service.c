#include "average_service.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_timer.h"

static const char *TAG = "AVG_SERVICE";

// Buffer circular para guardar as amostras
static float samples[SAMPLE_BUFFER_SIZE];
static int sample_index = 0;
static bool buffer_full = false;

// Variável para controlar o tempo (1 minuto)
static int64_t last_save_time = 0;
#define SAVE_INTERVAL_US 60000000 // 60 segundos em microsegundos

// Função Auxiliar: Salvar na Flash (NVS)
void save_average_to_flash(float average) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao abrir NVS!");
        return;
    }

    // Salva o valor com a chave "media_temp"
    // (Em um projeto real, mudariamos a chave conforme o sensor)
    // Convertendo float para blob ou int, pois NVS não tem float nativo simples
    // Aqui vamos multiplicar por 100 e salvar como inteiro para simplificar
    int32_t value_to_save = (int32_t)(average * 100);
    
    err = nvs_set_i32(my_handle, "avg_val", value_to_save);
    if (err == ESP_OK) {
        err = nvs_commit(my_handle);
        ESP_LOGI(TAG, "Media %.2f salva na Flash com sucesso!", average);
    }
    nvs_close(my_handle);
}

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

// A TASK PRINCIPAL
void average_task(void *pvParameters) {
    // Recebe o "handle" da fila como parâmetro
    QueueHandle_t inputQueue = (QueueHandle_t)pvParameters;
    sensor_data_t receivedData;
    
    last_save_time = esp_timer_get_time();

    while (1) {
        // 1. Espera chegar algo na fila (Timeout de 100ms para não travar)
        if (xQueueReceive(inputQueue, &receivedData, pdMS_TO_TICKS(100)) == pdTRUE) {
            
            // 2. Adiciona ao Buffer Circular
            samples[sample_index] = receivedData.sensor_value;
            sample_index++;
            
            // Se chegou no fim, volta pro começo (Circular)
            if (sample_index >= SAMPLE_BUFFER_SIZE) {
                sample_index = 0;
                buffer_full = true;
            }
            ESP_LOGI(TAG, "Amostra recebida: %.2f", receivedData.sensor_value);
        }

        // 3. Verifica se passou 1 minuto
        if ((esp_timer_get_time() - last_save_time) > SAVE_INTERVAL_US) {
            float avg = calculate_moving_average();
            ESP_LOGI(TAG, "Calculando media movel: %.2f", avg);
            
            save_average_to_flash(avg);
            
            last_save_time = esp_timer_get_time();
        }
    }
}

void init_nvs_storage(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}