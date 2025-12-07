#ifndef ALERTS_H
#define ALERTS_H

#include <stdbool.h>
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Configurações de Pinos
#define PIN_LED_GREEN   GPIO_NUM_16
#define PIN_LED_YELLOW  GPIO_NUM_17
#define PIN_LED_RED     GPIO_NUM_5
#define PIN_BUZZER      GPIO_NUM_18

// Configurações do Buzzer
#define BUZZER_MODE     LEDC_LOW_SPEED_MODE
#define BUZZER_TIMER    LEDC_TIMER_0
#define BUZZER_CHANNEL  LEDC_CHANNEL_0
#define BUZZER_RES      LEDC_TIMER_13_BIT
#define BUZZER_FREQ     4000

// Limites de Temperatura e Umidade
#define TEMP_MIN 18.0
#define TEMP_MAX 26.0
#define HUM_MIN  40.0
#define HUM_MAX  70.0
#define WARN_OFFSET_TEMP 2.0
#define WARN_OFFSET_HUM  10.0

// Estados do Sistema
typedef enum {
    STATE_NORMAL = 0,
    STATE_WARNING,
    STATE_CRITICAL
} system_state_t;

// Estrutura de dados para o DHT (o que vai na fila 1)
typedef struct {
    float temperature;
    float humidity;
} sensor_reading_t;

// Handles das Filas (Globais para serem acessíveis na main e tasks)
extern QueueHandle_t xSensorQueue;
extern QueueHandle_t xAlertQueue;

// Protótipos
void init_gpio();
void init_buzzer();
void set_buzzer_tone(int duty);

// Tasks
void vTaskSensorLogic(void *pvParameters);
void vTaskAlerts(void *pvParameters);

#endif