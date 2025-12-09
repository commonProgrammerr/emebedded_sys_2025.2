#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h" // Necessário para criar a Queue
#include "esp_log.h"

// Seus includes originais
#include "bh1750fvi_sensor.h"
#include "dht11_sensor.h"
#include "KY-037_sensor.h"
#include "sensor_monitor.h"

// Includes das Novas Tasks (#11 e #33)
#include "average_service.h"
#include "button_interface.h"

#define MIC_ADC_PIN 33
#define MIC_ADC_CHANEL ADC_CHANNEL_5
#define SPEAKER_PIN 34
#define RED_LED_PIN 26
#define YELLOW_LED_PIN 32
#define DHT11_PIN 23
#define BUTTON_PIN 22 // Nota: button_interface.h deve gerenciar os pinos agora

void app_main(void)
{
    // 1. Inicializar NVS (Obrigatório para a Task #33 salvar na Flash)
    init_nvs_storage();

    // 2. Inicializar Botões (Task #11)
    buttons_init();

    sensor_base_t bh1750 = {0}, dht11 = {0}, ky_037 = {0};
    SensorStatus_t status = bh1750fvi_init(&bh1750, SDA_IO, SCL_IO, BH1750_I2C_ADDR_LOW, BH1750_CONT_H_RES);
    dht11_init(&dht11, DHT11_PIN, 5000);
    KY037_init(&ky_037, MIC_ADC_CHANEL);
    
    if (status != SENSOR_OK)
    {
        printf("Falha ao inicializar o sensor BH1750. Abortando.\n");
        return;
    }

    // 3. CRIAR A FILA (O elo entre os sensores e a média móvel)
    // Cria uma fila capaz de segurar 10 itens do tipo sensor_data_t
    QueueHandle_t xSensorQueue = xQueueCreate(10, sizeof(sensor_data_t));

    // 4. Configurar Monitores (Passando a Queue onde estava NULL)
    sensor_monitor_t* th_monitor = new_sensor_monitor(
        &dht11,
        10000,
        sizeof(float),
        "temp&humidity_monitor",
        xSensorQueue // <--- AQUI: Agora envia dados para a fila!
    );
    
    // Nota: Se quiser média do ruído ou luz, passe xSensorQueue neles também.
    // Por enquanto, deixei NULL nos outros para focar na temperatura/umidade.
    sensor_monitor_t* noise_monitor = new_sensor_monitor(
        &ky_037,
        10000,
        sizeof(float),
        "noise_monitor",
        NULL 
    );
    sensor_monitor_t* light_monitor = new_sensor_monitor(
        &bh1750,
        10000,
        sizeof(float),
        "light_monitor",
        NULL
    );

    start_sensor_monitoring(th_monitor);
    start_sensor_monitoring(noise_monitor);
    start_sensor_monitoring(light_monitor);

    // 5. Criar a Task de Média Móvel (Consome a fila)
    xTaskCreate(average_task, "AvgService", 4096, (void *)xSensorQueue, 4, NULL);

    // 6. Loop Principal (Atualizado para processar botões)
    while (1) {
        buttons_process();             // Mantém a interface de botões viva
        vTaskDelay(pdMS_TO_TICKS(10)); // Delay para não travar a CPU
    }
}