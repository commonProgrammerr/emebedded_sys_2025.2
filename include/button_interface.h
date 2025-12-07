#ifndef BUTTON_INTERFACE_H
#define BUTTON_INTERFACE_H

#include <stdint.h>
#include "driver/gpio.h"

// Definição do Pino Único (Usando GPIO 21)
#define BUTTON_PIN     GPIO_NUM_21

// Estados do Sistema
typedef enum {
    STATE_NORMAL,
    STATE_CONFIG_TEMP,
    STATE_CONFIG_HUMIDITY
} sys_state_t;

void buttons_init(void);
void buttons_process(void);
sys_state_t get_current_state(void);

#endif