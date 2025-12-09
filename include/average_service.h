#ifndef AVERAGE_SERVICE_H
#define AVERAGE_SERVICE_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Tamanho do buffer pedido na task
#define SAMPLE_BUFFER_SIZE 6

// Estrutura para os dados que vêm da Queue
// (Estou assumindo que a queue envia um float com o valor do sensor)
typedef struct {
    float sensor_value;
    int sensor_id; // 0 para Temp, 1 para Umidade, etc.
} sensor_data_t;

// Função principal da Task
void average_task(void *pvParameters);

// Inicializa a NVS (Memória Flash)
void init_nvs_storage(void);

#endif