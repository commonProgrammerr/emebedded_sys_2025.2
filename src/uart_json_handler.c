#include "uart_json_handler.h"

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
    char json_buf[128];
    int len = snprintf(json_buf, sizeof(json_buf),
                       "{\"time\":%u,\"temp\":%.2f,\"humi\":%.2f,\"light\":%u,\"noise\":%u}",
                       (unsigned)record->id,
                       (double)record->temperature,
                       (double)record->humidity,
                       (unsigned)record->light,
                       (unsigned)record->noise);

    if (len <= 0) {
        ESP_LOGE(TAG, "Erro ao formatar JSON");
        return UART_JSON_ERROR;
    }

    uart_json_status_t status = uart_json_transmit((uint8_t*)json_buf, (uint32_t)len);
    if (status == UART_JSON_OK) {
        uart_json_transmit((uint8_t*)"\n", 1);
    }

    return status;
}

/**
 * @brief Envia um array de records como JSON via UART
 */
uart_json_status_t uart_json_send_history(const read_record_t* history, uint16_t count) {
    if (history == NULL || count == 0) {
        return UART_JSON_INVALID_INPUT;
    }

    /* Stream the JSON array using direct UART writes to avoid filling the
       circular buffer when sending large histories. This blocks on the
       UART mutex so the caller will wait until all bytes are written. */
    uart_json_status_t status;
    status = uart_json_write_direct((const uint8_t*)"[", 1);
    if (status != UART_JSON_OK) return status;

    char json_buf[128];
    for (uint16_t i = 0; i < count; i++) {
        if (i > 0) {
            status = uart_json_write_direct((const uint8_t*)",", 1);
            if (status != UART_JSON_OK) return status;
        }

        int len = snprintf(json_buf, sizeof(json_buf),
                           "{\"time\":%u,\"temp\":%.2f,\"humi\":%.2f,\"light\":%u,\"noise\":%u}",
                           (unsigned)history[i].id,
                           (double)history[i].temperature,
                           (double)history[i].humidity,
                           (unsigned)history[i].light,
                           (unsigned)history[i].noise);

        if (len <= 0) {
            ESP_LOGE(TAG, "Erro ao formatar JSON para record %u", (unsigned)i);
            return UART_JSON_ERROR;
        }

        status = uart_json_write_direct((const uint8_t*)json_buf, (uint32_t)len);
        if (status != UART_JSON_OK) return status;
    }

    status = uart_json_write_direct((const uint8_t*)"]", 1);
    if (status == UART_JSON_OK) {
        uart_json_write_direct((const uint8_t*)"\n", 1);
    }

    ESP_LOGI(TAG, "Histórico de %d registros enviado", count);
    return status;
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
