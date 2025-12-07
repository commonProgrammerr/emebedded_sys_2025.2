#include "alerts.h"
#include "esp_log.h"

static const char *TAG = "ALERTS_LOGIC";

// Definição das filas
QueueHandle_t xSensorQueue = NULL;
QueueHandle_t xAlertQueue = NULL;

void init_gpio() {
    gpio_reset_pin(PIN_LED_GREEN);
    gpio_reset_pin(PIN_LED_YELLOW);
    gpio_reset_pin(PIN_LED_RED);
    
    gpio_set_direction(PIN_LED_GREEN, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_LED_YELLOW, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_LED_RED, GPIO_MODE_OUTPUT);
}

void init_buzzer() {
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = BUZZER_MODE,
        .timer_num        = BUZZER_TIMER,
        .duty_resolution  = BUZZER_RES,
        .freq_hz          = BUZZER_FREQ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .speed_mode     = BUZZER_MODE,
        .channel        = BUZZER_CHANNEL,
        .timer_sel      = BUZZER_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PIN_BUZZER,
        .duty           = 0,
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);
}

void set_buzzer_tone(int duty) {
    ledc_set_duty(BUZZER_MODE, BUZZER_CHANNEL, duty);
    ledc_update_duty(BUZZER_MODE, BUZZER_CHANNEL);
}

/**
 * @brief Lógica do sensor
 * Consome dados brutos da xSensorQueue -> Processa -> Envia estado para xAlertQueue
 */
void vTaskSensorLogic(void *pvParameters) {
    sensor_reading_t reading;
    int violation_counter = 0;
    
    // Mantemos o estado localmente para lógica de histerese
    system_state_t current_logic_state = STATE_NORMAL;

    for (;;) {
        // Bloqueia esperando dados do sensor (timeout infinito ou longo)
        if (xQueueReceive(xSensorQueue, &reading, portMAX_DELAY) == pdTRUE) {
            
            float temp = reading.temperature;
            float hum = reading.humidity;
            
            system_state_t new_calculated_state = STATE_NORMAL;

            bool temp_critical = (temp < (TEMP_MIN - WARN_OFFSET_TEMP)) || (temp > (TEMP_MAX + WARN_OFFSET_TEMP));
            bool hum_critical = (hum < (HUM_MIN - WARN_OFFSET_HUM)) || (hum > (HUM_MAX + WARN_OFFSET_HUM));
            bool temp_warning = !temp_critical && ((temp < TEMP_MIN) || (temp > TEMP_MAX));
            bool hum_warning = !hum_critical && ((hum < HUM_MIN) || (hum > HUM_MAX));

            if (temp_critical || hum_critical) new_calculated_state = STATE_CRITICAL;
            else if (temp_warning || hum_warning) new_calculated_state = STATE_WARNING;
            else new_calculated_state = STATE_NORMAL;

            // Lógica de Debounce/Contador de Violação
            if (new_calculated_state != STATE_NORMAL) violation_counter++;
            else violation_counter = 0;

            // Só muda o estado se persistir por 3 leituras ou se voltar ao normal
            if (violation_counter >= 3 || new_calculated_state == STATE_NORMAL) {
                current_logic_state = new_calculated_state;
            }

            ESP_LOGI(TAG, "Processado: T:%.1f H:%.1f | Estado: %d | Violacoes: %d", 
                     temp, hum, current_logic_state, violation_counter);

            // Envia o estado processado para a task de Alertas
            // Usamos xQueueOverwrite para garantir que o alerta tenha sempre o estado mais recente
            // (Requer fila de tamanho 1)
            xQueueOverwrite(xAlertQueue, &current_logic_state);
        }
    }
}

/**
 * @brief Task de Controle de Atuadores
 * Lê o estado da xAlertQueue e controla LEDs/Buzzer
 */
void vTaskAlerts(void *pvParameters) {
    int toggle_counter = 0;
    system_state_t active_state = STATE_NORMAL; // Estado local da task

    for (;;) {
        // Verifica se há um novo estado na fila. 
        // Delay 0 = não bloqueia. Se não tiver nada, continua com o estado anterior.
        system_state_t received_state;
        if (xQueueReceive(xAlertQueue, &received_state, 0) == pdTRUE) {
            active_state = received_state;
            ESP_LOGD(TAG, "Estado atualizado nos alertas: %d", active_state);
        }

        // Reseta tudo antes de aplicar a lógica do ciclo atual
        gpio_set_level(PIN_LED_GREEN, 0);
        gpio_set_level(PIN_LED_YELLOW, 0);
        gpio_set_level(PIN_LED_RED, 0);
        set_buzzer_tone(0); 

        switch (active_state) {
            case STATE_NORMAL:
                gpio_set_level(PIN_LED_GREEN, 1);
                break;
                
            case STATE_WARNING:
                // Pisca Amarelo (Frequência média)
                gpio_set_level(PIN_LED_YELLOW, (toggle_counter % 10) < 5); 
                break;
                
            case STATE_CRITICAL:
                // Pisca Vermelho rápido + Buzzer
                bool state_on = (toggle_counter % 5) < 2; 
                gpio_set_level(PIN_LED_RED, state_on);
                if (state_on) set_buzzer_tone(4000); 
                break;
        }

        toggle_counter++;
        vTaskDelay(pdMS_TO_TICKS(100)); // Base de tempo da animação
    }
}