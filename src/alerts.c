#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "alerts.h"


volatile system_state_t current_state = STATE_NORMAL;

static const char *TAG = "BIOTERIO";

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

void vTaskSensorLogic(void *pvParameters) {
    float temp = 22.0;
    float hum = 50.0;
    int violation_counter = 0;

    for (;;) {
        // TODO: Leitura DHT11
        temp += 0.5; 
        if(temp > 30) temp = 18; 

        system_state_t new_state = STATE_NORMAL;

        bool temp_critical = (temp < (TEMP_MIN - WARN_OFFSET_TEMP)) || (temp > (TEMP_MAX + WARN_OFFSET_TEMP));
        bool hum_critical = (hum < (HUM_MIN - WARN_OFFSET_HUM)) || (hum > (HUM_MAX + WARN_OFFSET_HUM));
        bool temp_warning = !temp_critical && ((temp < TEMP_MIN) || (temp > TEMP_MAX));
        bool hum_warning = !hum_critical && ((hum < HUM_MIN) || (hum > HUM_MAX));

        if (temp_critical || hum_critical) new_state = STATE_CRITICAL;
        else if (temp_warning || hum_warning) new_state = STATE_WARNING;
        else new_state = STATE_NORMAL;

        if (new_state != STATE_NORMAL) violation_counter++;
        else violation_counter = 0;

        if (violation_counter >= 3 || new_state == STATE_NORMAL) {
            current_state = new_state;
        }

        ESP_LOGI(TAG, "Leitura: T:%.1f H:%.1f | Estado: %d | Violacoes: %d", temp, hum, current_state, violation_counter);
        vTaskDelay(pdMS_TO_TICKS(2000)); 
    }
}

void vTaskAlerts(void *pvParameters) {
    int toggle_counter = 0;

    for (;;) {
        gpio_set_level(PIN_LED_GREEN, 0);
        gpio_set_level(PIN_LED_YELLOW, 0);
        gpio_set_level(PIN_LED_RED, 0);
        set_buzzer_tone(0); 

        switch (current_state) {
            case STATE_NORMAL:
                gpio_set_level(PIN_LED_GREEN, 1);
                vTaskDelay(pdMS_TO_TICKS(100)); 
                break;
            case STATE_WARNING:
                gpio_set_level(PIN_LED_YELLOW, (toggle_counter % 10) < 5); 
                vTaskDelay(pdMS_TO_TICKS(100)); 
                break;
            case STATE_CRITICAL:
                bool state_on = (toggle_counter % 5) < 2; 
                gpio_set_level(PIN_LED_RED, state_on);
                if (state_on) set_buzzer_tone(4000); 
                vTaskDelay(pdMS_TO_TICKS(100));
                break;
        }
        toggle_counter++;
    }
}

void app_main() {
    init_gpio();
    init_buzzer();

    ESP_LOGI(TAG, "Boot Test...");
  
    gpio_set_level(PIN_LED_GREEN, 1); vTaskDelay(pdMS_TO_TICKS(300));
    gpio_set_level(PIN_LED_YELLOW, 1); vTaskDelay(pdMS_TO_TICKS(300));
    gpio_set_level(PIN_LED_RED, 1); vTaskDelay(pdMS_TO_TICKS(300));
    gpio_set_level(PIN_LED_GREEN, 0); 
    gpio_set_level(PIN_LED_YELLOW, 0); 
    gpio_set_level(PIN_LED_RED, 0);

    xTaskCreate(vTaskSensorLogic, "SensorLogic", 4096, NULL, 5, NULL);
    xTaskCreate(vTaskAlerts, "Alerts", 2048, NULL, 5, NULL);
}