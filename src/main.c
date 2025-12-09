#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "bh1750fvi_sensor.h"
#include "dht11_sensor.h"
#include "KY-037_sensor.h"
#include "sensor_monitor.h"

#include "average_service.h"

// Removidos NVS e Botões conforme pedido do mentor

#define MIC_ADC_PIN 33
#define MIC_ADC_CHANEL ADC_CHANNEL_5
#define DHT11_PIN 23

// 1. Declarar a Fila como GLOBAL para o callback enxergar
QueueHandle_t xSensorQueue;

// 2. Criar a função Callback
// Essa é a função que o monitor vai chamar quando ler um dado.
// Ela pega o dado e joga na fila.
void sensor_callback(void *data) {
    if (xSensorQueue != NULL && data != NULL) {
        float *value = (float*)data;
        sensor_data_t msg;
        msg.sensor_value = *value;
        msg.sensor_id = 0; // Id genérico por enquanto
        
        // Envia para a fila (sem bloquear muito tempo)
        xQueueSend(xSensorQueue, &msg, 0);
    }
}

void app_main(void) {
    // Inicialização de HW básica
    // (Removido init_nvs e buttons_init)
    
    sensor_base_t bh1750 = {0}, dht11 = {0}, ky_037 = {0};
    // ... inits dos sensores mantidos iguais ...
    bh1750fvi_init(&bh1750, SDA_IO, SCL_IO, BH1750_I2C_ADDR_LOW, BH1750_CONT_H_RES);
    dht11_init(&dht11, DHT11_PIN, 5000);
    KY037_init(&ky_037, MIC_ADC_CHANEL);

    // 3. Inicializar a Fila Global
    xSensorQueue = xQueueCreate(10, sizeof(sensor_data_t));

    // 4. Passar o CALLBACK (sensor_callback) e não a fila
    sensor_monitor_t* th_monitor = new_sensor_monitor(
        &dht11,
        10000,
        sizeof(float),
        "temp&humidity_monitor",
        sensor_callback // <--- AGORA ESTÁ CERTO! Passamos a função.
    );
    
    // Fazemos o mesmo para os outros se quisermos média deles também
    sensor_monitor_t* noise_monitor = new_sensor_monitor(
        &ky_037, 10000, sizeof(float), "noise_monitor", sensor_callback
    );
    sensor_monitor_t* light_monitor = new_sensor_monitor(
        &bh1750, 10000, sizeof(float), "light_monitor", sensor_callback
    );

    start_sensor_monitoring(th_monitor);
    start_sensor_monitoring(noise_monitor);
    start_sensor_monitoring(light_monitor);

    // 5. Task da Média consome a mesma fila global
    xTaskCreate(average_task, "AvgService", 4096, (void *)xSensorQueue, 4, NULL);

    while (1) {
        vTaskDelay(portMAX_DELAY);
    }
}