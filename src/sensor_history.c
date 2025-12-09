#include "sensor_history.h"

static compact_sensor_read_t *history_buffer = NULL;
static size_t history_size = 0;
static size_t max_history_records = 0;
static size_t current_record_index = 0;

void init_history_system(size_t max_records) {
    if (history_buffer) {
        free(history_buffer);
    }
    history_size = max_records * sizeof(compact_sensor_read_t);
    history_buffer = malloc(history_size);
    if (history_buffer) {
        memset(history_buffer, 0, history_size);
        max_history_records = max_records;
        current_record_index = 0;
    }
}
uint32_t get_moving_average(compact_sensor_read_t *avg) {
    if (!history_buffer || max_history_records == 0) {
        return 0;
    }

    int64_t sum_temp = 0;
    uint64_t sum_humid = 0;
    uint64_t sum_lux = 0;
    uint64_t sum_noise = 0;
    size_t count = 0;

    for (size_t i = 0; i < max_history_records; i++) {
        compact_sensor_read_t *record = &history_buffer[i];
        // Considera apenas registros válidos (não zero)
        if (record->temperature != 0 || record->humidity != 0 ||
            record->lux != 0 || record->noise_level != 0) {
            sum_temp += record->temperature;
            sum_humid += record->humidity;
            sum_lux += record->lux;
            sum_noise += record->noise_level;
            count++;
        }
    }

    if (count > 0) {
        avg->temperature = sum_temp / count;
        avg->humidity = sum_humid / count;
        avg->lux = sum_lux / count;
        avg->noise_level = sum_noise / count;
        return count;
    }
    return 0;
}
void save_sensor_read(const full_sensor_read_t *read) {
    if (!history_buffer || max_history_records == 0 || !read) {
        return;
    }

    compact_sensor_read_t compact;
    full_to_compact(read, &compact);

    // Salva no índice atual
    history_buffer[current_record_index] = compact;

    // Atualiza o índice circularmente
    current_record_index = (current_record_index + 1) % max_history_records;
}

void compact_to_full(const compact_sensor_read_t *compact, full_sensor_read_t *full) {
    if (!compact || !full) {
        return;
    }
    full->temperature = RAW_TO_TEMP(compact->temperature);
    full->humidity = RAW_TO_HUMID(compact->humidity);
    full->lux = RAW_TO_LUX(compact->lux);
    full->noise_level = RAW_TO_NOISE(compact->noise_level);
}

void full_to_compact(const full_sensor_read_t *full, compact_sensor_read_t *compact) {
    if (!full || !compact) {
        return;
    }
    compact->temperature = TEMP_TO_RAW(full->temperature);
    compact->humidity = HUMID_TO_RAW(full->humidity);
    compact->lux = LUX_TO_RAW(full->lux);
    compact->noise_level = NOISE_TO_RAW(full->noise_level);
}