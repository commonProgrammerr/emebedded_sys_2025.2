#include "uart_json_handler.h"
#include "flash_buffer.h"
#include "flash_record.h"
#include "sensor_history.h"
#include "esp_system.h"
#include "esp_timer.h"

/*
 * Note: other platform includes, defines and typedefs were moved to the header
 * to centralize configuration and meet project guidelines.
 */

static const char *TAG = "UART_JSON";

static circular_buffer_t tx_buffer = {0};
static SemaphoreHandle_t uart_mutex = NULL;

/**
 * @brief Inicializa o buffer circular TX
 */
uart_json_status_t circular_buffer_init(void) {
    memset(&tx_buffer, 0, sizeof(circular_buffer_t));
    tx_buffer.write_idx = 0;
    tx_buffer.read_idx = 0;
    tx_buffer.count = 0;
    
    tx_buffer.mutex = xSemaphoreCreateMutex();
    if (tx_buffer.mutex == NULL) {
        ESP_LOGE(TAG, "Falha ao criar mutex do buffer circular");
        return UART_JSON_ERROR;
    }
    
    ESP_LOGI(TAG, "Buffer circular inicializado (512 bytes)");
    return UART_JSON_OK;
}

/**
 * @brief Adiciona dados ao buffer circular TX
 */
static uart_json_status_t circular_buffer_write(const uint8_t* data, uint16_t len) {
    if (data == NULL || len == 0) {
        return UART_JSON_INVALID_INPUT;
    }
    
    if (tx_buffer.mutex == NULL) {
        return UART_JSON_ERROR;
    }
    
    if (xSemaphoreTake(tx_buffer.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Timeout ao tentar acessar mutex do buffer");
        return UART_JSON_TX_BUSY;
    }
    
    // Verifica se há espaço suficiente
    if ((tx_buffer.count + len) > TX_BUFFER_SIZE) {
        xSemaphoreGive(tx_buffer.mutex);
        ESP_LOGW(TAG, "Buffer circular cheio. Disponível: %d bytes, Necessário: %d bytes", 
                 TX_BUFFER_SIZE - tx_buffer.count, len);
        return UART_JSON_BUFFER_FULL;
    }
    
    // Escreve os dados no buffer
    for (uint16_t i = 0; i < len; i++) {
        tx_buffer.buffer[tx_buffer.write_idx] = data[i];
        tx_buffer.write_idx = (tx_buffer.write_idx + 1) % TX_BUFFER_SIZE;
        tx_buffer.count++;
    }
    
    xSemaphoreGive(tx_buffer.mutex);
    return UART_JSON_OK;
}

/**
 * @brief Lê dados do buffer circular TX
 */
static uint16_t circular_buffer_read(uint8_t* dest, uint16_t max_len) {
    if (dest == NULL || max_len == 0) {
        return 0;
    }
    
    if (tx_buffer.mutex == NULL) {
        return 0;
    }
    
    if (xSemaphoreTake(tx_buffer.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return 0;
    }
    
    uint16_t bytes_to_read = (tx_buffer.count < max_len) ? tx_buffer.count : max_len;
    
    for (uint16_t i = 0; i < bytes_to_read; i++) {
        dest[i] = tx_buffer.buffer[tx_buffer.read_idx];
        tx_buffer.read_idx = (tx_buffer.read_idx + 1) % TX_BUFFER_SIZE;
        tx_buffer.count--;
    }
    
    xSemaphoreGive(tx_buffer.mutex);
    return bytes_to_read;
}

/**
 * @brief Inicializa a UART com configuração 115200, 8N1
 */
uart_json_status_t uart_json_init(void) {
    uart_config_t uart_config = {
        .baud_rate = UART_BAUDRATE,
        .data_bits = UART_DATA_BITS,
        .parity = UART_PARITY,
        .stop_bits = UART_STOP_BITS,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    
    // Configura a UART
    if (uart_param_config(UART_PORT, &uart_config) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao configurar parâmetros da UART");
        return UART_JSON_ERROR;
    }
    
    // Define os pinos de TX/RX
    if (uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao configurar pinos da UART");
        return UART_JSON_ERROR;
    }
    
    // Instala o driver UART com tamanho de buffer de 1024 bytes
    if (uart_driver_install(UART_PORT, 1024, TX_BUFFER_SIZE, 0, NULL, 0) != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao instalar driver UART");
        return UART_JSON_ERROR;
    }
    
    // Inicializa o buffer circular
    if (circular_buffer_init() != UART_JSON_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar buffer circular");
        return UART_JSON_ERROR;
    }
    
    // Cria mutex para sincronização
    uart_mutex = xSemaphoreCreateMutex();
    if (uart_mutex == NULL) {
        ESP_LOGE(TAG, "Falha ao criar mutex UART");
        return UART_JSON_ERROR;
    }
    
    ESP_LOGI(TAG, "UART inicializada: 115200, 8N1");
    return UART_JSON_OK;
}

/**
 * @brief Envia dados via UART usando o buffer circular
 */
static uart_json_status_t uart_json_transmit(const uint8_t* data, uint32_t len) {
    if (data == NULL || len == 0) {
        return UART_JSON_INVALID_INPUT;
    }
    
    // Adiciona dados ao buffer circular
    uart_json_status_t status = circular_buffer_write(data, len);
    if (status != UART_JSON_OK) {
        return status;
    }
    
    // Envia dados do buffer circular pela UART
    uint8_t temp_buffer[256];
    uint16_t bytes_read = circular_buffer_read(temp_buffer, sizeof(temp_buffer));
    
    if (bytes_read > 0) {
        if (xSemaphoreTake(uart_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            // Se não conseguir acessar, recoloca os dados no buffer
            circular_buffer_write(temp_buffer, bytes_read);
            return UART_JSON_TX_BUSY;
        }
        
        int written = uart_write_bytes(UART_PORT, (const char*)temp_buffer, bytes_read);
        xSemaphoreGive(uart_mutex);
        
        if (written < 0) {
            ESP_LOGE(TAG, "Erro ao escrever na UART");
            return UART_JSON_ERROR;
        }
    }
    
    return UART_JSON_OK;
}

/**
 * @brief Escreve diretamente na UART segurando o mutex (bypass do buffer circular)
 */
static uart_json_status_t uart_json_write_direct(const uint8_t* data, uint32_t len) {
    if (data == NULL || len == 0) return UART_JSON_INVALID_INPUT;
    if (uart_mutex == NULL) return UART_JSON_ERROR;

    // Bloqueia até conseguir o mutex para garantir sequência
    if (xSemaphoreTake(uart_mutex, portMAX_DELAY) != pdTRUE) {
        return UART_JSON_TX_BUSY;
    }

    int written = uart_write_bytes(UART_PORT, (const char*)data, len);
    xSemaphoreGive(uart_mutex);

    if (written < 0) return UART_JSON_ERROR;
    return UART_JSON_OK;
}

/**
 * @brief Converte um record para JSON e envia via UART
 */
uart_json_status_t uart_json_send_record(const read_record_t* record) {
    if (record == NULL) {
        return UART_JSON_INVALID_INPUT;
    }
    
    if (xSemaphoreTake(uart_mutex, portMAX_DELAY) != pdTRUE) {
        return UART_JSON_TX_BUSY;
    }
    
    printf("{\"time\":%u,\"temp\":%.2f,\"humi\":%.2f,\"light\":%u,\"noise\":%u}\n",
           (unsigned)record->id,
           (double)record->temperature,
           (double)record->humidity,
           (unsigned)record->light,
           (unsigned)record->noise);
    
    xSemaphoreGive(uart_mutex);
    return UART_JSON_OK;
}

/**
 * @brief Envia um array de records como JSON via UART
 */
uart_json_status_t uart_json_send_history(const read_record_t* history, uint16_t count) {
    if (history == NULL || count == 0) {
        return UART_JSON_INVALID_INPUT;
    }

    if (xSemaphoreTake(uart_mutex, portMAX_DELAY) != pdTRUE) {
        return UART_JSON_TX_BUSY;
    }

    printf("[");
    for (uint16_t i = 0; i < count; i++) {
        if (i > 0) printf(",");
        printf("{\"time\":%u,\"temp\":%.2f,\"humi\":%.2f,\"light\":%u,\"noise\":%u}",
               (unsigned)history[i].id,
               (double)history[i].temperature,
               (double)history[i].humidity,
               (unsigned)history[i].light,
               (unsigned)history[i].noise);
    }
    printf("]\n");

    xSemaphoreGive(uart_mutex);
    ESP_LOGI(TAG, "Histórico de %d registros enviado", count);
    return UART_JSON_OK;
}

/**
 * @brief Envia uma string JSON customizada via UART
 */
uart_json_status_t uart_json_send_string(const char* json_string) {
    if (json_string == NULL) {
        return UART_JSON_INVALID_INPUT;
    }
    
    uart_json_status_t status = uart_json_transmit((uint8_t*)json_string, strlen(json_string));
    
    if (status == UART_JSON_OK) {
        uart_json_transmit((uint8_t*)"\n", 1);
    }
    
    return status;
}

/**
 * @brief Processa dados recebidos da UART
 */
uart_json_status_t uart_json_process_received(const uint8_t* buffer, size_t length) {
    if (buffer == NULL || length == 0) {
        return UART_JSON_INVALID_INPUT;
    }
    
    ESP_LOGI(TAG, "Dados recebidos: %.*s", length, buffer);
    
    // Aqui você pode adicionar lógica para processar comandos JSON recebidos
    // Por enquanto, apenas logamos os dados
    
    return UART_JSON_OK;
}

/**
 * @brief Obtém o espaço disponível no buffer circular TX
 */
int32_t circular_buffer_available_space(void) {
    if (tx_buffer.mutex == NULL) {
        return -1;
    }
    
    if (xSemaphoreTake(tx_buffer.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return -1;
    }
    
    int32_t available = TX_BUFFER_SIZE - tx_buffer.count;
    xSemaphoreGive(tx_buffer.mutex);
    
    return available;
}

/**
 * @brief Obtém o espaço ocupado no buffer circular TX
 */
int32_t circular_buffer_used_space(void) {
    if (tx_buffer.mutex == NULL) {
        return -1;
    }
    
    if (xSemaphoreTake(tx_buffer.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return -1;
    }
    
    int32_t used = tx_buffer.count;
    xSemaphoreGive(tx_buffer.mutex);
    
    return used;
}

/**
 * @brief Limpa o buffer circular TX
 */
uart_json_status_t circular_buffer_clear(void) {
    if (tx_buffer.mutex == NULL) {
        return UART_JSON_ERROR;
    }
    
    if (xSemaphoreTake(tx_buffer.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return UART_JSON_TX_BUSY;
    }
    
    memset(tx_buffer.buffer, 0, TX_BUFFER_SIZE);
    tx_buffer.write_idx = 0;
    tx_buffer.read_idx = 0;
    tx_buffer.count = 0;
    
    xSemaphoreGive(tx_buffer.mutex);
    
    ESP_LOGI(TAG, "Buffer circular limpo");
    return UART_JSON_OK;
}

/**
 * @brief Rotina de interrupt da UART
 */
void uart_interrupt_handler(void) {
    // Esta função pode ser expandida para lidar com interrupções da UART
    // Por enquanto, mantém a estrutura básica
}

/**
 * @brief Lê todo o conteúdo do buffer de flash, envia via UART em JSON,
 *        limpa os históricos (RAM e flash) e reinicia o dispositivo.
 */
uart_json_status_t uart_json_dump_flash_and_restart(void)
{
    flash_buffer_t *fb = flash_buffer_get_global();
    if (!fb) {
        ESP_LOGE(TAG, "Flash buffer global não inicializado");
        return UART_JSON_ERROR;
    }

    uint32_t count = flash_buffer_get_count(fb);
    if (count == 0) {
        ESP_LOGI(TAG, "Nenhuma amostra na flash para dump");
        return UART_JSON_OK;
    }

    // Aloca buffer para leitura
    flash_record_t *records = malloc(sizeof(flash_record_t) * count);
    if (!records) {
        ESP_LOGE(TAG, "Falha ao alocar memória para leitura da flash");
        return UART_JSON_ERROR;
    }

    uint32_t read = flash_buffer_read(fb, records, count);
    if (read == 0) {
        ESP_LOGW(TAG, "Nenhuma amostra lida da flash (count=%d)", count);
        free(records);
        return UART_JSON_ERROR;
    }

    // Envia JSON array (mais antigo primeiro)
    if (xSemaphoreTake(uart_mutex, portMAX_DELAY) != pdTRUE) {
        free(records);
        return UART_JSON_TX_BUSY;
    }

    printf("[");
    for (int i = (int)read - 1; i >= 0; i--) {
        // Converte compacto -> full
        full_sensor_read_t full = {0};
        compact_to_full(&records[i].compact, &full);

        if (i != (int)read - 1) {
            printf(",");
        }

        printf("{\"time\":%u,\"temp\":%.2f,\"humi\":%.2f,\"light\":%.2f,\"noise\":%u}",
               (unsigned)records[i].timestamp,
               (double)full.temperature,
               (double)full.humidity,
               (double)full.lux,
               (unsigned)full.noise_level);
    }
    printf("]\n");

    xSemaphoreGive(uart_mutex);

    free(records);

    // Limpa histórico em RAM
    init_history_system(60);
    // Limpa flash
    esp_err_t err = flash_buffer_clear(fb);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Falha ao limpar flash buffer: %s", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "Dump concluído, reiniciando dispositivo em 500ms...");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();

    return UART_JSON_OK; // não alcançado
}
