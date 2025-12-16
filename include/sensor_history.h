#ifndef SENSOR_HISTORY_H
#define SENSOR_HISTORY_H
#include <stdint.h>
#include <stdlib.h>

// Conversion macros
#define TEMP_TO_RAW(temp) ((int16_t)(temp))
#define RAW_TO_TEMP(raw) ((float)(raw))

#define HUMID_TO_RAW(humid) ((uint16_t)((humid)))
#define RAW_TO_HUMID(raw) ((float)(raw))

#define LUX_TO_RAW(lux) ((uint16_t)(lux) * 2.0f)
#define RAW_TO_LUX(raw) ((float)(raw) / 2.0f)

#define NOISE_TO_RAW(noise) ((uint8_t)(noise))
#define RAW_TO_NOISE(raw) ((float)(raw))

// Compact data structure using bit-fields (4 bytes total)
typedef struct compact_sensor_read {
    uint8_t temperature : 6;   // 0 to 50,    step 1.0   -> 51 values (needs 6 bits: 2^6=64)
    uint8_t humidity : 7;      // 0 to 100,   step 1.0   -> 101 values (needs 7 bits: 2^7=128)
    uint16_t lux : 12;         // 0 to 2047,  step 0.5   -> 4096 values (needs 12 bits: 2^12=4096)
    uint8_t noise_level : 7;   // 0 to 100,   step 1.0   -> 101 values (needs 7 bits: 2^7=128)
} __packed compact_sensor_read_t;

typedef struct full_sensor_read {
    float temperature;   // in °C
    float humidity;      // in %
    float lux;           // in lx
    float noise_level;   // in RMS %
} full_sensor_read_t;


void init_history_system(size_t max_records);
uint32_t get_moving_average(compact_sensor_read_t *avg);
void save_sensor_read(const full_sensor_read_t *read);
void compact_to_full(const compact_sensor_read_t *compact, full_sensor_read_t *full);
void full_to_compact(const full_sensor_read_t *full, compact_sensor_read_t *compact);


#endif // SENSOR_HISTORY_H