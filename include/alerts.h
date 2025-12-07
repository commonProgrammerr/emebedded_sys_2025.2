#ifndef ALERTS_SYSTEM_H 
#define ALERTS_SYSTEM_H

#include "driver/gpio.h"
#include "driver/ledc.h"

#define PIN_LED_GREEN    GPIO_NUM_18
#define PIN_LED_YELLOW   GPIO_NUM_19
#define PIN_LED_RED      GPIO_NUM_21
#define PIN_BUZZER       GPIO_NUM_23

#define TEMP_MIN         20.0
#define TEMP_MAX         24.0
#define HUM_MIN          40.0
#define HUM_MAX          70.0
#define WARN_OFFSET_TEMP 2.0  
#define WARN_OFFSET_HUM  10.0 
#define HYSTERESIS_TEMP  1.0

#define BUZZER_TIMER     LEDC_TIMER_0
#define BUZZER_MODE      LEDC_LOW_SPEED_MODE
#define BUZZER_CHANNEL   LEDC_CHANNEL_0
#define BUZZER_RES       LEDC_TIMER_13_BIT
#define BUZZER_FREQ      2000 


typedef enum {
    STATE_NORMAL,
    STATE_WARNING,
    STATE_CRITICAL
} system_state_t;


extern volatile system_state_t current_state;

void init_gpio(void);
void init_buzzer(void);
void set_buzzer_tone(int duty);


void vTaskSensorLogic(void *pvParameters);
void vTaskAlerts(void *pvParameters);

#endif 