#include "button_interface.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "BTN_INTERFACE";

// Variáveis de Estado
static sys_state_t current_state = STATE_NORMAL;
static int64_t last_interaction_time = 0; // Para o timeout de 30s

// Constantes de Tempo (em microsegundos)
#define TIMEOUT_INACTIVITY_US  30000000 // 30 segundos
#define LONG_PRESS_TIME_US      2000000 // 2 segundos

// Inicialização dos GPIOs
void buttons_init(void) {
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL<<BTN_UP_PIN) | (1ULL<<BTN_DOWN_PIN) | (1ULL<<BTN_SELECT_PIN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1; // PULL-UP ATIVADO (Pressionado = 0)
    gpio_config(&io_conf);
    
    last_interaction_time = esp_timer_get_time();
    ESP_LOGI(TAG, "Interface de Botoes Inicializada");
}

sys_state_t get_current_state(void) {
    return current_state;
}

// Lógica de processamento de um botão específico
// Retorna: 0=Nada, 1=Click Curto, 2=Long Press
int check_button(int pin) {
    if (gpio_get_level(pin) == 0) { // Botão pressionado (Low)
        vTaskDelay(pdMS_TO_TICKS(50)); // DEBOUNCE 50ms (Requisito da Task)
        
        if (gpio_get_level(pin) == 0) { // Ainda pressionado?
            int64_t press_start = esp_timer_get_time();
            bool long_press_detected = false;

            // Espera soltar ou dar o tempo de long press
            while (gpio_get_level(pin) == 0) {
                vTaskDelay(pdMS_TO_TICKS(10));
                
                // Checa Long Press (2 segundos)
                if (!long_press_detected && (esp_timer_get_time() - press_start > LONG_PRESS_TIME_US)) {
                    long_press_detected = true;
                    ESP_LOGI(TAG, "Evento: Long Press Detectado!");
                    return 2; // Código para Long Press
                }
            }
            // Se soltou antes de 2s, é click curto
            if (!long_press_detected) {
                return 1; // Código para Click Curto
            }
        }
    }
    return 0; // Nada aconteceu
}

void buttons_process(void) {
    int64_t now = esp_timer_get_time();

    // 1. Verificar Timeout (30s sem atividade volta ao Normal)
    if (current_state != STATE_NORMAL && (now - last_interaction_time > TIMEOUT_INACTIVITY_US)) {
        ESP_LOGW(TAG, "Timeout de Inatividade! Retornando ao estado NORMAL.");
        current_state = STATE_NORMAL;
        last_interaction_time = now; // Reset timer
    }

    // 2. Processar Botão SELECT (Navegação)
    int event_select = check_button(BTN_SELECT_PIN);
    if (event_select > 0) { // Se houve click ou long press
        last_interaction_time = esp_timer_get_time(); // Reseta timeout
        
        if (event_select == 1) { // Click Curto: Avança estado
            switch (current_state) {
                case STATE_NORMAL:
                    current_state = STATE_CONFIG_TEMP;
                    ESP_LOGI(TAG, "Mudou para: CONFIG TEMP");
                    break;
                case STATE_CONFIG_TEMP:
                    current_state = STATE_CONFIG_HUMIDITY;
                    ESP_LOGI(TAG, "Mudou para: CONFIG UMIDADE");
                    break;
                case STATE_CONFIG_HUMIDITY:
                    current_state = STATE_NORMAL;
                    ESP_LOGI(TAG, "Mudou para: NORMAL");
                    break;
            }
        } else if (event_select == 2) {
            // Se a task pedir algo específico para long press no select, coloque aqui
            ESP_LOGI(TAG, "Long Press no Select (Sem ação definida)");
        }
    }

    // 3. Processar Botão UP (Ajuste +1)
    if (check_button(BTN_UP_PIN) == 1) {
        last_interaction_time = esp_timer_get_time();
        if (current_state == STATE_CONFIG_TEMP) {
            ESP_LOGI(TAG, "Aumentar Temp (+1)");
            // Aqui você chamaria uma função para mudar a variável real de temperatura
        } else if (current_state == STATE_CONFIG_HUMIDITY) {
            ESP_LOGI(TAG, "Aumentar Umidade (+1)");
        }
    }

    // 4. Processar Botão DOWN (Ajuste -1)
    if (check_button(BTN_DOWN_PIN) == 1) {
        last_interaction_time = esp_timer_get_time();
        if (current_state == STATE_CONFIG_TEMP) {
            ESP_LOGI(TAG, "Diminuir Temp (-1)");
        } else if (current_state == STATE_CONFIG_HUMIDITY) {
            ESP_LOGI(TAG, "Diminuir Umidade (-1)");
        }
    }
}