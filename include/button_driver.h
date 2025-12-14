#ifndef BUTTON_DRIVER_H
#define BUTTON_DRIVER_H

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Tipos de eventos que o botão pode gerar
typedef enum
{
    BUTTON_PRESS_NULL = 0,
    BUTTON_PRESS_SHORT,  // Toque rápido
    BUTTON_PRESS_NORMAL, // Toque normal (ex: entre 500ms e 2s)
    BUTTON_PRESS_LONG    // Toque longo (ex: > 2s)
} button_event_t;

// Definição do Tipo de Função Callback
// (Quem usar o botão precisa criar uma função com essa assinatura)
typedef void (*button_callback_t)(int pin, button_event_t event);

// A "Classe" Botão
typedef struct
{
    gpio_num_t pin;              // O pino físico
    button_callback_t callback;  // A função que será chamada quando ocorrer evento
    TaskHandle_t task_to_notify; // Task que receberá o notify

    // Variáveis internas de controle (privadas na lógica)
    int64_t press_start_time;
    bool is_pressed;
} Button_t;

// Inicializa o botão com interrupção
void button_init(Button_t *btn, gpio_num_t pin, button_callback_t cb, TaskHandle_t task_handle);

// Task interna que processa os eventos (chamada automaticamente)
void button_task(void *pvParameters);

#endif