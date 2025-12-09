#ifndef BUTTON_DRIVER_H
#define BUTTON_DRIVER_H

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Tipos de eventos que o botão pode gerar
typedef enum {
    BUTTON_PRESS_SHORT,  // Toque rápido
    BUTTON_PRESS_NORMAL, // Toque normal (ex: entre 500ms e 2s)
    BUTTON_PRESS_LONG    // Toque longo (ex: > 2s)
} button_event_t;

// Definição do Tipo de Função Callback
// (Quem usar o botão precisa criar uma função com essa assinatura)
typedef void (*button_callback_t)(int pin, button_event_t event);

// A "Classe" Botão
typedef struct {
    gpio_num_t pin;             // O pino físico
    button_callback_t callback; // A função que será chamada quando ocorrer evento
    
    // Variáveis internas de controle (privadas na lógica)
    int64_t press_start_time;
    bool is_pressed;
} Button_t;

// Construtor: Inicializa o botão
void button_init(Button_t *btn, gpio_num_t pin, button_callback_t cb);

// Loop: Deve ser chamado periodicamente para checar esse botão
void button_process(Button_t *btn);

#endif