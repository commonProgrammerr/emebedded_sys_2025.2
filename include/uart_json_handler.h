#ifndef UART_JSON_HANDLER_H
#define UART_JSON_HANDLER_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Configurações UART
#define UART_PORT UART_NUM_0
#define UART_BAUDRATE 115200
#define UART_TX_PIN 1
#define UART_RX_PIN 3
#define UART_DATA_BITS UART_DATA_8_BITS
#define UART_STOP_BITS UART_STOP_BITS_1
#define UART_PARITY UART_PARITY_DISABLE

// Buffer Circular TX
#define TX_BUFFER_SIZE 512

typedef struct {
    uint8_t buffer[TX_BUFFER_SIZE];
    uint16_t write_idx;
    uint16_t read_idx;
    uint16_t count;
    SemaphoreHandle_t mutex;
} circular_buffer_t;

/**
 * @brief Estrutura para um registro de leitura de sensores
 */
typedef struct read_record {
    uint16_t id;
    float temperature;
    float humidity;
    uint16_t noise;
    uint16_t light;
} read_record_t;

/**
 * @brief Tipos de retorno para as funções UART/JSON
 */
typedef enum {
    UART_JSON_OK = 0,
    UART_JSON_ERROR = -1,
    UART_JSON_BUFFER_FULL = -2,
    UART_JSON_INVALID_INPUT = -3,
    UART_JSON_TX_BUSY = -4
} uart_json_status_t;

/**
 * @brief Inicializa o módulo UART com configuração 115200, 8N1
 * 
 * @return uart_json_status_t Status da inicialização
 */
uart_json_status_t uart_json_init(void);

/**
 * @brief Inicializa o buffer circular TX (512 bytes)
 * 
 * @return uart_json_status_t Status da inicialização
 */
uart_json_status_t circular_buffer_init(void);

/**
 * @brief Envia um array de registros de sensores como JSON via UART
 * 
 * Formato esperado:
 * [{"time":123456,"temp":22.5,"humi":55.0,"light":0,"noise":12}, ...]
 * 
 * @param history Array de registros de sensores
 * @param count Quantidade de registros no array
 * @return uart_json_status_t Status do envio
 */
uart_json_status_t uart_json_send_history(const read_record_t* history, uint16_t count);

/**
 * @brief Envia um único registro de sensor como JSON via UART
 * 
 * @param record Ponteiro para o registro a ser enviado
 * @return uart_json_status_t Status do envio
 */
uart_json_status_t uart_json_send_record(const read_record_t* record);

/**
 * @brief Envia uma string JSON customizada via UART
 * 
 * @param json_string Ponteiro para a string JSON
 * @return uart_json_status_t Status do envio
 */
uart_json_status_t uart_json_send_string(const char* json_string);

/**
 * @brief Processa dados recebidos da UART
 * 
 * @param buffer Buffer com os dados recebidos
 * @param length Tamanho dos dados
 * @return uart_json_status_t Status do processamento
 */
uart_json_status_t uart_json_process_received(const uint8_t* buffer, size_t length);

/**
 * @brief Obtém o espaço disponível no buffer circular TX
 * 
 * @return int32_t Bytes disponíveis ou -1 em caso de erro
 */
int32_t circular_buffer_available_space(void);

/**
 * @brief Obtém o espaço ocupado no buffer circular TX
 * 
 * @return int32_t Bytes ocupados ou -1 em caso de erro
 */
int32_t circular_buffer_used_space(void);

/**
 * @brief Limpa o buffer circular TX
 * 
 * @return uart_json_status_t Status da operação
 */
uart_json_status_t circular_buffer_clear(void);

/**
 * @brief Rotina de interrupt da UART (deve ser chamada pelo driver UART)
 */
void uart_interrupt_handler(void);

#endif // UART_JSON_HANDLER_H
