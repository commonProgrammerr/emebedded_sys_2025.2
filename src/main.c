#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "alerts.h"
#include "sensor_monitor.h"

// Supondo que você tenha um driver wrapper para o DHT compatível com sensor_base_t
// #include "dht_sensor_wrapper.h" 

static const char *TAG = "MAIN";

// --- CALLBACK DO SENSOR ---
// Esta função é chamada automaticamente pela task do sensor_monitor
void dht_data_ready_callback(sensor_base_t* sensor, void* data) {
    // Cast dos dados (assumindo que seu driver DHT retorna essa estrutura)
    // Se o seu driver retorna outra coisa, ajuste aqui.
    sensor_reading_t* reading = (sensor_reading_t*)data;
    
    // Envia para a Fila de Lógica
    // Delay 0 pois estamos dentro de um callback que roda na task de leitura
    if (xQueueSend(xSensorQueue, reading, 0) != pdPASS) {
        ESP_LOGW(TAG, "Fila de sensores cheia, dado descartado");
    }
}

void app_main() {
    ESP_LOGI(TAG, "Inicializando Hardware...");
    init_gpio();
    init_buzzer();

    // 1. Criação das Filas
    // Fila para dados do sensor (Buffer pequeno é suficiente se a leitura for lenta)
    xSensorQueue = xQueueCreate(5, sizeof(sensor_reading_t));
    
    // Fila para estados de alerta (Tamanho 1 pois usamos xQueueOverwrite)
    xAlertQueue = xQueueCreate(1, sizeof(system_state_t));

    if (xSensorQueue == NULL || xAlertQueue == NULL) {
        ESP_LOGE(TAG, "Falha ao criar filas");
        return;
    }

    // 2. Setup do Sensor Monitor (Exemplo)
    // sensor_base_t* my_dht = dht_sensor_create(GPIO_NUM_4, DHT_TYPE_DHT11);
    
    // sensor_monitor_t* monitor = new_sensor_monitor(
    //     my_dht,
    //     2000,                       // 2 segundos
    //     sizeof(sensor_reading_t),   // Tamanho dos dados
    //     "DHT11_Main",
    //     dht_data_ready_callback     // <--- AQUI LIGAMOS O CALLBACK
    // );
    // start_sensor_monitoring(monitor);

    ESP_LOGI(TAG, "Iniciando Tasks...");

    // Cria as tasks de Lógica e Alerta
    xTaskCreate(vTaskSensorLogic, "SensorLogic", 4096, NULL, 5, NULL);
    xTaskCreate(vTaskAlerts, "Alerts", 2048, NULL, 5, NULL);
    
    // Para teste sem o sensor físico, você pode criar uma task temporária que
    // injeta dados na fila para ver o LED mudar:
    /*
    xTaskCreate(vTaskDebugInject, "DebugInject", 2048, NULL, 1, NULL);
    */
}