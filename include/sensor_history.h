#ifndef SENSOR_HISTORY_H
#define SENSOR_HISTORY_H
#include <stdint.h>
#include <stdlib.h>

// Conversion macros
#define TEMP_TO_RAW(temp) ((int16_t)((temp) * 100.0f))
#define RAW_TO_TEMP(raw) ((float)(raw) / 100.0f)

#define HUMID_TO_RAW(humid) ((uint16_t)((humid) - 20.0f))
#define RAW_TO_HUMID(raw) ((float)(raw) + 20.0f)

#define LUX_TO_RAW(lux) ((uint16_t)((lux) * 10.0f))
#define RAW_TO_LUX(raw) ((float)(raw) / 10.0f)

#define NOISE_TO_RAW(noise) ((uint16_t)((noise) / 5))
#define RAW_TO_NOISE(raw) ((float)(raw) * 5)

// Compact data structure using bit-fields (5 bytes total)
typedef struct compact_sensor_read {
    int16_t temperature : 9;   // -50 to 50, step 0.01 -> range -5000 to 5000 (needs 9 bits signed)
    uint16_t humidity : 7;     // 20 to 90, step 1.0 -> range 70 values (needs 7 bits)
    uint16_t lux : 14;         // 0 to 1024, step 0.1 -> range 10240 values (needs 14 bits)
    uint16_t noise_level : 10; // 0 to 4098, step 5.0 -> range ~820 values (needs 10 bits)
} compact_sensor_read_t;

typedef struct full_sensor_read {
    float temperature;   // in °C
    float humidity;      // in %
    float lux;          // in lx
    uint16_t noise_level;  // raw ADC value
} full_sensor_read_t;


void init_history_system(size_t max_records);
uint32_t get_moving_average(compact_sensor_read_t *avg);
void save_sensor_read(const full_sensor_read_t *read);
void compact_to_full(const compact_sensor_read_t *compact, full_sensor_read_t *full);
void full_to_compact(const full_sensor_read_t *full, compact_sensor_read_t *compact);


#endif // SENSOR_HISTORY_H