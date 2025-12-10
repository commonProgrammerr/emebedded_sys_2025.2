#ifndef FLASH_RECORD_H
#define FLASH_RECORD_H

#include "sensor_history.h"
#include <stdint.h>

/**
 * @brief Estrutura armazenada na flash para cada amostra
 *
 * Contém timestamp em segundos (desde boot ou epoch, conforme disponibilidade)
 * seguido da leitura compactada (economiza espaço).
 */
typedef struct flash_record {
    uint32_t timestamp;               // seconds since boot (esp_timer_get_time()/1e6)
    compact_sensor_read_t compact;    // leitura compactada
} flash_record_t;

#endif // FLASH_RECORD_H
