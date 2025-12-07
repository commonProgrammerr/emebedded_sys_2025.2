    #include <stdio.h>
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
    #include "esp_log.h" // For ESP-IDF logging
    #include "esp32-dht11.h"

    void myTask(void *pvParameters) {
        for (;;) {
            ESP_LOGI("MyTask", "Hello from FreeRTOS task!");
            vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second
        }
    }

    void app_main() {
        xTaskCreatePinnedToCore(
            myTask,         // Task function
            "MyTask",       // Task name
            2048,           // Stack size (bytes)
            NULL,           // Parameters
            5,              // Priority
            NULL,           // Task handle
            0               // Core to run on (0 or 1)
        );
    }