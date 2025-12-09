#include "button_driver.h"
#include "esp_timer.h"
#include "esp_log.h"

// Tempos em microssegundos
#define DEBOUNCE_TIME_US   50000   // 50ms
#define LONG_PRESS_US      2000000 // 2 segundos
#define NORMAL_PRESS_US    500000  // 0.5 segundos

void button_init(Button_t *btn, gpio_num_t pin, button_callback_t cb) {
    btn->pin = pin;
    btn->callback = cb;
    btn->is_pressed = false;
    btn->press_start_time = 0;

    // Configuração física do GPIO
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << pin);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1; // Pull-up (Pressionado = 0)
    gpio_config(&io_conf);
}

void button_process(Button_t *btn) {
    int level = gpio_get_level(btn->pin);
    int64_t now = esp_timer_get_time();

    // Lógica de Estado
    if (level == 0) { // Botão Pressionado (Low)
        if (!btn->is_pressed) {
            // Detectou borda de descida (Início do toque)
            // Aqui poderia ter um debounce simples
            btn->is_pressed = true;
            btn->press_start_time = now;
        }
    } else { // Botão Solto (High)
        if (btn->is_pressed) {
            // Detectou borda de subida (Fim do toque)
            btn->is_pressed = false;
            
            // Calcula quanto tempo ficou segurado
            int64_t duration = now - btn->press_start_time;

            if (duration < DEBOUNCE_TIME_US) {
                // Foi ruído, ignora
                return; 
            }

            // Classifica o evento e CHAMA O CALLBACK
            if (btn->callback != NULL) {
                if (duration > LONG_PRESS_US) {
                    btn->callback(btn->pin, BUTTON_PRESS_LONG);
                } else if (duration > NORMAL_PRESS_US) {
                    btn->callback(btn->pin, BUTTON_PRESS_NORMAL);
                } else {
                    btn->callback(btn->pin, BUTTON_PRESS_SHORT);
                }
            }
        }
    }
}