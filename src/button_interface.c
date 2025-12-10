#include "button_interface.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "uart_json_handler.h"

static const char *TAG = "BTN_INTERFACE";

static sys_state_t current_state = STATE_NORMAL;
static int64_t last_interaction_time = 0; 

#define TIMEOUT_INACTIVITY_US  30000000 // 30 segundos
#define LONG_PRESS_TIME_US      2000000 // 2 segundos

void buttons_init(void) {
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    // Máscara de bits apenas para o botão único
    io_conf.pin_bit_mask = (1ULL<<BUTTON_PIN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1; // Pull-up ligado
    gpio_config(&io_conf);
    
    last_interaction_time = esp_timer_get_time();
    ESP_LOGI(TAG, "Botao Unico Inicializado no pino %d", BUTTON_PIN);
}

sys_state_t get_current_state(void) {
    return current_state;
}

// Função genérica de checagem (mantida igual, útil para callbacks futuros)
int check_button(int pin) {
    if (gpio_get_level(pin) == 0) { 
        vTaskDelay(pdMS_TO_TICKS(50)); // Debounce
        
        if (gpio_get_level(pin) == 0) {
            int64_t press_start = esp_timer_get_time();
            bool long_press_detected = false;

            while (gpio_get_level(pin) == 0) {
                vTaskDelay(pdMS_TO_TICKS(10));
                if (!long_press_detected && (esp_timer_get_time() - press_start > LONG_PRESS_TIME_US)) {
                    long_press_detected = true;
                    ESP_LOGI(TAG, "Long Press Detectado!");
                    return 2; 
                }
            }
            if (!long_press_detected) return 1; 
        }
    }
    return 0;
}

void buttons_process(void) {
    int64_t now = esp_timer_get_time();

    // 1. Verificar Timeout
    if (current_state != STATE_NORMAL && (now - last_interaction_time > TIMEOUT_INACTIVITY_US)) {
        ESP_LOGW(TAG, "Timeout! Voltando ao normal.");
        current_state = STATE_NORMAL;
        last_interaction_time = now;
    }

    // 2. Processar Botão Único
    int event = check_button(BUTTON_PIN);
    
    if (event > 0) {
        last_interaction_time = esp_timer_get_time();
        
        if (event == 1) { // Click Curto: Cicla entre os estados
            switch (current_state) {
                case STATE_NORMAL:
                    current_state = STATE_CONFIG_TEMP;
                    ESP_LOGI(TAG, "Estado: CONFIG TEMP");
                    break;
                case STATE_CONFIG_TEMP:
                    current_state = STATE_CONFIG_HUMIDITY;
                    ESP_LOGI(TAG, "Estado: CONFIG UMIDADE");
                    break;
                case STATE_CONFIG_HUMIDITY:
                    current_state = STATE_NORMAL;
                    ESP_LOGI(TAG, "Estado: NORMAL");
                    break;
            }
        } else if (event == 2) {
            // Long press: realizar dump do histórico via UART, limpar e reiniciar
            ESP_LOGI(TAG, "Long Press: iniciando dump do historico e reinicio");
            if (uart_json_dump_flash_and_restart() != UART_JSON_OK) {
                ESP_LOGW(TAG, "Falha ao realizar dump via UART");
            }
        }
    }
}