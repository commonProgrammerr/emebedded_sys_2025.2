#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "bh1750fvi_sensor.h"
#include "sensor_monitor.h"

void app_main(void)
{
    sensor_base_t bh1750, dht11, ky_037;
    SensorStatus_t status = bh1750fvi_init(&bh1750, SDA_IO, SCL_IO, BH1750_I2C_ADDR_LOW, BH1750_CONT_H_RES);

    if (status != SENSOR_OK)
    {
        printf("Falha ao inicializar o sensor BH1750. Abortando.\n");
        return;
    }

    sensor_monitor_t* light_monitor = new_sensor_monitor(
        &bh1750,
        10000,
        sizeof(float),
        "light",
        NULL
    );

    start_sensor_monitoring(light_monitor);
    for(;;)
        vTaskDelay(portMAX_DELAY);
    
}