#ifndef UART_JSON_HANDLER_H
#define UART_JSON_HANDLER_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "flash_buffer.h"
#include "flash_record.h"


/**
 * @brief Envia um único registro de sensor como JSON via UART
 * 
 * @param record Ponteiro para o registro a ser enviado
 * @return uart_json_status_t Status do envio
 */
esp_err_t uart_json_send_record(const flash_record_t* record, char* term);

/**
 * @brief Lê todo o conteúdo do `flash_buffer` (registros timestamp+compact) e envia via UART
 *        Em seguida limpa o histórico (RAM e flash) e reinicia o dispositivo.
 */
esp_err_t uart_json_dump_flash_and_restart(flash_buffer_t *buffer);


#endif // UART_JSON_HANDLER_H
