#include "freertos/FreeRTOS.h"
#include "bh1750.h"
#include "bh1750fvi_sensor.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "bh1750";

SensorStatus_t bh1750fvi_init(sensor_base_t *self, uint8_t sda_io, uint8_t scl_io, uint8_t i2c_address, uint8_t mode)
{
    if (self == NULL)
    {
        ESP_LOGE(TAG, "Invalid address for sensor");
        return SENSOR_ERROR;
    }

    if (self->context != NULL)
        free(self->context);

    self->context = malloc(sizeof(bh1750fvi_context_t));
    if (self->context == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for context");
        return SENSOR_ERROR;
    }

    bh1750fvi_context_t *context = (bh1750fvi_context_t *)self->context;

    // Inicializa toda a estrutura com zeros primeiro
    memset(&context->i2c_bus_config, 0, sizeof(i2c_master_bus_config_t));
    
    context->i2c_bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    context->i2c_bus_config.i2c_port = 0; // I2C_NUM_0
    context->i2c_bus_config.scl_io_num = scl_io;
    context->i2c_bus_config.sda_io_num = sda_io;
    context->i2c_bus_config.glitch_ignore_cnt = 7;
    context->i2c_bus_config.flags.enable_internal_pullup = true; // Usa pull-ups internos

    esp_err_t err = i2c_new_master_bus(&context->i2c_bus_config, &context->bus_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Falha ao inicializar barramento I2C: %s", esp_err_to_name(err));
        free(self->context);
        self->context = NULL;
        return SENSOR_ERROR;
    }

    err = bh1750_init(context->bus_handle, i2c_address, &(context->bh1750));
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize BH1750: %s", esp_err_to_name(err));
        i2c_del_master_bus(context->bus_handle);
        free(self->context);
        self->context = NULL;
        return SENSOR_ERROR;
    }

    err = bh1750_set_measurement_mode(&context->bh1750, mode, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set measurement mode: %s", esp_err_to_name(err));
        bh1750_deinit(&context->bh1750);
        i2c_del_master_bus(context->bus_handle);
        free(self->context);
        self->context = NULL;
        return SENSOR_ERROR;
    }

    self->read_data = bh1750fvi_read_data;
    self->deinit = bh1750fvi_deinit;

    ESP_LOGI(TAG, "bh1750 sensor initialized successfully");
    return SENSOR_OK;
}

SensorStatus_t bh1750fvi_read_data(sensor_base_t *self, void *data)
{
    if (self == NULL || data == NULL)
    {
        ESP_LOGE(TAG, "Invalid parameters");
        return SENSOR_ERROR;
    }

    bh1750fvi_context_t *context = (bh1750fvi_context_t *)self->context;
    float lux = 0.0f;
    static uint8_t err_count;
    esp_err_t err = bh1750_read_lux(&context->bh1750, &lux);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read lux: %s", esp_err_to_name(err));
        if (++err_count > 0 && err_count % 10 == 0)
        {
            ESP_LOGW(TAG, "Reconfigurando sensor...");
            bh1750_reset(&context->bh1750);
            vTaskDelay(pdMS_TO_TICKS(10));
            bh1750_set_measurement_mode(&context->bh1750, context->bh1750.mode, 0);
            err_count = 0;
        };

        return SENSOR_ERROR;
    }
    memcpy(data, (float *)&lux, sizeof(float));
    ESP_LOGI(TAG, "Light level: %.2f lux", lux);
    return SENSOR_OK;
}

SensorStatus_t bh1750fvi_deinit(sensor_base_t *self)
{
    if (self == NULL)
    {
        return SENSOR_ERROR;
    }
    bh1750fvi_context_t *context = (bh1750fvi_context_t *)self->context;
    if (context)
    {
        bh1750_deinit(&context->bh1750);
        i2c_del_master_bus(context->bus_handle);
        free(self->context);
    }
    free(self);
    ESP_LOGI(TAG, "bh1750 sensor deinitialized");
    return SENSOR_OK;
}
