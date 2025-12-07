#ifndef BUTTON_INTERFACE_H
#define BUTTON_INTERFACE_H

#include <stdint.h>
#include "driver/gpio.h"

// Definição dos Pinos (Adaptado de PB0/PB1/PB2 para ESP32)
// Verifique se esses pinos estão livres na sua placa!
#define BTN_UP_PIN     GPIO_NUM_18
#define BTN_DOWN_PIN   GPIO_NUM_19
#define BTN_SELECT_PIN GPIO_NUM_21

// Estados do Sistema (Conforme a Task)
typedef enum {
    STATE_NORMAL,
    STATE_CONFIG_TEMP,
    STATE_CONFIG_HUMIDITY
} sys_state_t;

// Inicializa os pinos
void buttons_init(void);

// Função principal que deve ser chamada no loop/task
void buttons_process(void);

// Getter para saber o estado atual (útil para o main)
sys_state_t get_current_state(void);

#endif