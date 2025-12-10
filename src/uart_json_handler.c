#include "uart_json_handler.h"
#include "flash_buffer.h"
#include "flash_record.h"
#include "sensor_history.h"
#include "esp_system.h"
#include "esp_timer.h"

static const char *TAG = "UART_JSON";

static esp_err_t uart_json_send_record_chunk(const void* records, uint32_t samples);

/**
 * @brief Converte um record para JSON e envia via UART
 */
esp_err_t uart_json_send_record(const flash_record_t* record, char* term) {
    if (record == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (term == NULL) {
        term = "\n";
    }

    full_sensor_read_t full_read;
    compact_to_full(&record->compact, &full_read);

    printf("{\"timestamp\":%lu,\"temp\":%.2f,\"humi\":%.2f,\"light\":%.2f,\"noise\":%u}%s",
           record->timestamp,
            full_read.temperature,
           full_read.humidity,
           full_read.lux,
           full_read.noise_level, 
           term);

    return ESP_OK;
}

/**
 * @brief Lê todo o conteúdo do buffer de flash, envia via UART em JSON,
 *        limpa o históricos (flash) e reinicia o dispositivo.
 */
esp_err_t uart_json_dump_flash_and_restart(flash_buffer_t *buffer)
{
    if (!buffer) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t count = flash_buffer_get_count(buffer);
    if (count == 0) {
        ESP_LOGW(TAG, "Nenhuma amostra na flash para dump");
        return ESP_OK;
    }

    printf("[");
    flash_buffer_read_in_chunks(buffer, uart_json_send_record_chunk, 10);
    printf("]\n");

    // Limpa flash
    esp_err_t err = flash_buffer_clear(buffer);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Falha ao limpar flash buffer: %s", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "Dump concluído, reiniciando dispositivo em 500ms...");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();

    return ESP_OK; // não alcançado
}

static esp_err_t uart_json_send_record_chunk(const void* records, uint32_t samples) {
    if (records == NULL || samples == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    flash_record_t* recs = (flash_record_t*)records;
    esp_err_t err;
    for (size_t i = 0; i < samples; i++) {
        err = uart_json_send_record(&recs[i], ",\n");
        if (err != ESP_OK) {
            return err;
        }
    }

    return ESP_OK;
}
