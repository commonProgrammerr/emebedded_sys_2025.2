/**
 * @file sensor_flash_buffer.h
 * @brief Buffer circular não volátil na flash memory usando NVS
 * 
 * Este módulo implementa um buffer circular que salva amostras de sensores
 * na flash memory do ESP32 de forma não volátil usando NVS (Non-Volatile Storage).
 * 
 * CARACTERÍSTICAS:
 * - Armazenamento persistente (dados sobrevivem a resets)
 * - Buffer circular com tamanho configurável
 * - Wear leveling automático gerenciado pela ESP-IDF
 * - API simples para salvar e recuperar amostras
 * 
 * COMO USAR:
 * 
 * 1. Inicialize o buffer:
 *    flash_buffer_t* buffer = flash_buffer_init("samples", 100);
 * 
 * 2. Salve amostras:
 *    sensor_read_t sample = {.temperature = 25.5, .humidity = 60.0, ...};
 *    flash_buffer_write(buffer, &sample);
 * 
 * 3. Leia amostras:
 *    sensor_read_t samples[10];
 *    uint32_t count = flash_buffer_read(buffer, samples, 10);
 * 
 * 4. Obtenha estatísticas:
 *    uint32_t total = flash_buffer_get_count(buffer);
 * 
 * 5. Limpe o buffer (opcional):
 *    flash_buffer_clear(buffer);
 * 
 * LIMITAÇÕES:
 * - Tamanho máximo de amostra: 512 bytes
 * - Tamanho máximo do buffer: limitado pelo NVS namespace (cerca de 4KB)
 * - Escrita na flash é mais lenta que RAM (use para dados importantes)
 */

#ifndef SENSOR_FLASH_BUFFER_H
#define SENSOR_FLASH_BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_err.h"

/**
 * @brief Handle do buffer na flash
 */
typedef struct {
    nvs_handle_t nvs_handle;    // Handle do NVS
    char namespace[16];      // Nome do namespace NVS
    uint32_t max_samples;        // Número máximo de amostras
    uint32_t write_index;        // Índice de escrita (circular)
    uint32_t sample_count;       // Número atual de amostras
    size_t sample_size;          // Tamanho de cada amostra
} flash_buffer_t;

/**
 * @brief Inicializa o sistema NVS (chame uma vez no início)
 * @return ESP_OK se sucesso
 */
esp_err_t flash_buffer_system_init(void);

/**
 * @brief Cria e inicializa um buffer na flash
 * @param namespace Nome do namespace NVS (máx 15 caracteres)
 * @param max_samples Número máximo de amostras a armazenar
 * @return Ponteiro para o buffer ou NULL em caso de erro
 */
flash_buffer_t* flash_buffer_init(const char* namespace, size_t sample_size, uint32_t max_samples);

/**
 * @brief Escreve uma amostra no buffer (sobrescreve a mais antiga se cheio)
 * @param buffer Ponteiro para o buffer
 * @param sample Ponteiro para a amostra a ser salva
 * @return ESP_OK se sucesso
 */
esp_err_t flash_buffer_write(flash_buffer_t* buffer, const void* sample);

/**
 * @brief Lê múltiplas amostras do buffer (as mais recentes)
 * @param buffer Ponteiro para o buffer
 * @param samples Array para armazenar as amostras lidas
 * @param count Número máximo de amostras a ler
 * @return Número real de amostras lidas
 */
uint32_t flash_buffer_read(flash_buffer_t* buffer, void* samples, uint32_t count);

/**
 * @brief Lê a última amostra gravada
 * @param buffer Ponteiro para o buffer
 * @param sample Ponteiro para armazenar a amostra
 * @return true se sucesso, false se buffer vazio
 */
bool flash_buffer_read_last(flash_buffer_t* buffer, void* sample);

/**
 * @brief Retorna o número de amostras armazenadas
 * @param buffer Ponteiro para o buffer
 * @return Número de amostras
 */
uint32_t flash_buffer_get_count(const flash_buffer_t* buffer);

/**
 * @brief Verifica se o buffer está cheio
 * @param buffer Ponteiro para o buffer
 * @return true se cheio
 */
bool flash_buffer_is_full(const flash_buffer_t* buffer);

/**
 * @brief Limpa todas as amostras do buffer
 * @param buffer Ponteiro para o buffer
 * @return ESP_OK se sucesso
 */
esp_err_t flash_buffer_clear(flash_buffer_t* buffer);

/**
 * @brief Fecha e libera o buffer
 * @param buffer Ponteiro para o buffer
 */
void flash_buffer_deinit(flash_buffer_t* buffer);

#endif // SENSOR_FLASH_BUFFER_H
