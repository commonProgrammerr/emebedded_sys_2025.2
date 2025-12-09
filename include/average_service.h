#ifndef AVERAGE_SERVICE_H
#define AVERAGE_SERVICE_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define SAMPLE_BUFFER_SIZE 6

typedef struct {
    float sensor_value;
    int sensor_id;
} sensor_data_t;

void average_task(void *pvParameters);

#endif