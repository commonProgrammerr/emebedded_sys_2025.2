#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "alerts.h"

static const char *TAG = "MAIN";

void app_main() {

    init_gpio();
    init_buzzer();

    ESP_LOGI(TAG, "Teste de Boot...");

    gpio_set_level(PIN_LED_GREEN, 1); vTaskDelay(pdMS_TO_TICKS(200));
    gpio_set_level(PIN_LED_YELLOW, 1); vTaskDelay(pdMS_TO_TICKS(200));
    gpio_set_level(PIN_LED_RED, 1); vTaskDelay(pdMS_TO_TICKS(200));
    gpio_set_level(PIN_LED_GREEN, 0); gpio_set_level(PIN_LED_YELLOW, 0); gpio_set_level(PIN_LED_RED, 0);

    ESP_LOGI(TAG, "Iniciando");

    xTaskCreate(vTaskSensorLogic, "SensorLogic", 4096, NULL, 5, NULL);
    xTaskCreate(vTaskAlerts, "Alerts", 2048, NULL, 5, NULL);
}