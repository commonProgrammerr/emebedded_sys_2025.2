/**
 * @file sensor_monitor.h
 * @brief Sistema de monitoramento periódico de sensores usando FreeRTOS
 * 
 * Este módulo implementa um sistema de monitoramento que permite a leitura
 * periódica de sensores usando Software Timers e Task Notifications do FreeRTOS.
 * 
 * COMO USAR:
 * 
 * 1. Crie um sensor_base_t do seu sensor (DHT11, BH1750, etc)
 * 
 * 2. Crie um monitor para o sensor:
 *    sensor_monitor_t* monitor = new_sensor_monitor(
 *        sensor_base,           // Ponteiro para o sensor
 *        1000,                   // Intervalo em ms (1 segundo)
 *        sizeof(dht11_data_t),  // Tamanho dos dados do sensor
 *        "DHT11",               // Nome do sensor
 *        my_callback            // Função callback (pode ser NULL)
 *    );
 * 
 * 3. Inicie o monitoramento:
 *    start_sensor_monitoring(monitor);
 * 
 * 4. Para parar o monitoramento:
 *    stop_sensor_monitoring(monitor);     // Para um sensor específico
 *    stop_sensor_monitoring(NULL);        // Para todos os sensores
 * 
 * EXEMPLO DE CALLBACK:
 * 
 * void my_callback(sensor_base_t* sensor, void* data) {
 *     dht11_data_t* sensor_data = (dht11_data_t*)data;
 *     printf("Temp: %.2f°C, Humidity: %.2f%%\n", 
 *            sensor_data->temperature, sensor_data->humidity);
 * }
 * 
 * FUNCIONAMENTO INTERNO:
 * - Um Software Timer dispara periodicamente no intervalo configurado
 * - O timer notifica uma Task dedicada usando Task Notification
 * - A Task lê os dados do sensor e chama o callback se fornecido
 * - Cada sensor tem sua própria Task, mas compartilham o mesmo timer callback
 */

#ifndef SENSOR_MONITOR_H
#define SENSOR_MONITOR_H
#include <stdint.h>
#include "sensor_base.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"

/** Tag para logs do ESP-IDF */
#define SENSOR_MONITOR_TAG "SensorMonitor"

/** Intervalo padrão de monitoramento em milissegundos */
#define DEFAULT_SENSOR_MONITOR_INTERVAL_MS 1000

/** Número máximo de sensores que podem ser monitorados simultaneamente */
#define MAX_SENSORS 3

/**
 * @brief Callback chamado quando dados do sensor são lidos com sucesso
 * @param sensor Ponteiro para o sensor_base_t que foi lido
 * @param data Ponteiro para os dados lidos (tipo depende do sensor)
 */
typedef void (*sensor_data_callback_t)(sensor_base_t* sensor, void* data);

/**
 * @brief Estrutura que representa um monitor de sensor
 */
typedef struct {
    TimerHandle_t timer;              /**< Handle do Software Timer do FreeRTOS */
    TickType_t interval;              /**< Intervalo entre leituras em ticks */
    BaseType_t is_running;            /**< Flag indicando se o monitoramento está ativo */
    TaskHandle_t task_handle;         /**< Handle da task de leitura do sensor */
    sensor_base_t* sensor;            /**< Ponteiro para o sensor sendo monitorado */
    size_t data_size;                 /**< Tamanho da estrutura de dados do sensor */
    char *sensor_name;                /**< Nome do sensor (para logs) */
    sensor_data_callback_t data_callback; /**< Callback executado após leitura bem-sucedida */
} sensor_monitor_t;

/** Array global de sensores monitorados */
extern sensor_monitor_t* sensors[MAX_SENSORS];

/**
 * @brief Cria um novo monitor de sensor
 * @param sensor_base Ponteiro para o sensor a ser monitorado
 * @param interval_ms Intervalo de leitura em milissegundos
 * @param data_size Tamanho da estrutura de dados do sensor (ex: sizeof(dht11_data_t))
 * @param sensor_name Nome do sensor para identificação nos logs
 * @param callback Função callback chamada após leitura (pode ser NULL)
 * @return Ponteiro para o sensor_monitor_t criado, ou NULL em caso de erro
 */
sensor_monitor_t* new_sensor_monitor(sensor_base_t* sensor_base, TickType_t interval_ms, size_t data_size, const char* sensor_name, sensor_data_callback_t callback);

/**
 * @brief Inicia o monitoramento periódico de um sensor
 * @param sensor Ponteiro para o sensor_monitor_t a ser iniciado
 * @note Cria uma task dedicada e inicia o timer periódico
 */
void start_sensor_monitoring(sensor_monitor_t* sensor);

/**
 * @brief Para o monitoramento de um ou todos os sensores
 * @param sensor Ponteiro para o sensor específico, ou NULL para parar todos
 * @note Para o timer mas não deleta a task
 */
void stop_sensor_monitoring(sensor_monitor_t* sensor);

#endif // SENSOR_MONITOR_H