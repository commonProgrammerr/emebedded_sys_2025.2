#include "BH1750FVI_sensor.h"
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BH1750FVI";

// Estrutura para armazenar os parâmetros de inicialização
typedef struct BH1750FVI_context {
    i2c_port_t i2c_port;
    uint8_t i2c_address;
    uint8_t mode;
} BH1750FVI_context_t;

void BH1750FVI_setup_i2c(sensor_base_t* self) {
    BH1750FVI_sensor_t* bh1750 = (BH1750FVI_sensor_t*)self;

    // Configuração do I2C Master
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = GPIO_NUM_21,  // Ajuste conforme seu hardware
        .scl_io_num = GPIO_NUM_22,  // Ajuste conforme seu hardware
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,  // 100kHz
    };

    esp_err_t err = i2c_param_config(bh1750->i2c_port, &conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure I2C parameters");
        return;
    }

    err = i2c_driver_install(bh1750->i2c_port, conf.mode, 0, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install I2C driver");
        return;
    }

    ESP_LOGI(TAG, "I2C initialized successfully");
}

static esp_err_t BH1750FVI_write_command(BH1750FVI_sensor_t* bh1750, uint8_t command) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (bh1750->i2c_address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, command, true);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(bh1750->i2c_port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    
    return ret;
}

static esp_err_t BH1750FVI_read_raw(BH1750FVI_sensor_t* bh1750, uint16_t* raw_value) {
    uint8_t data[2];
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (bh1750->i2c_address << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, &data[0], I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &data[1], I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(bh1750->i2c_port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    
    if (ret == ESP_OK) {
        *raw_value = (data[0] << 8) | data[1];
    }
    
    return ret;
}

static SensorStatus_t BH1750FVI_init_wrapper(sensor_base_t* self) {
    BH1750FVI_context_t* context = (BH1750FVI_context_t*)self->context;
    return BH1750FVI_init(self, context->i2c_port, context->i2c_address, context->mode);
}

SensorStatus_t BH1750FVI_init(sensor_base_t* self, i2c_port_t i2c_port, uint8_t i2c_address, uint8_t mode) {
    if (self == NULL) {
        self = (sensor_base_t*)malloc(sizeof(BH1750FVI_sensor_t));
        if (self == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for sensor");
            return SENSOR_ERROR;
        }
    }

    BH1750FVI_sensor_t* bh1750 = (BH1750FVI_sensor_t*)self;
    bh1750->i2c_port = i2c_port;
    bh1750->i2c_address = i2c_address;
    bh1750->mode = mode;

    // Configurar os ponteiros de função
    self->init = BH1750FVI_init_wrapper;
    self->read_data = BH1750FVI_read_data;
    self->deinit = BH1750FVI_deinit;

    // Salvar contexto
    if (self->context == NULL) {
        self->context = malloc(sizeof(BH1750FVI_context_t));
        if (self->context == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for context");
            free(self);
            return SENSOR_ERROR;
        }
        BH1750FVI_context_t* context = (BH1750FVI_context_t*)self->context;
        context->i2c_port = i2c_port;
        context->i2c_address = i2c_address;
        context->mode = mode;
    }

    // Configurar I2C
    BH1750FVI_setup_i2c(self);

    // Power ON
    esp_err_t err = BH1750FVI_write_command(bh1750, BH1750_POWER_ON);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to power on sensor");
        return SENSOR_ERROR;
    }

    // Pequeno delay após power on
    vTaskDelay(pdMS_TO_TICKS(10));

    // Configurar modo de medição
    err = BH1750FVI_write_command(bh1750, mode);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set measurement mode");
        return SENSOR_ERROR;
    }

    // Delay para a primeira medição (depende do modo)
    if (mode == BH1750_CONTINUOUS_H_RES || mode == BH1750_CONTINUOUS_H_RES2 ||
        mode == BH1750_ONE_TIME_H_RES || mode == BH1750_ONE_TIME_H_RES2) {
        vTaskDelay(pdMS_TO_TICKS(180));  // 120ms + margem
    } else {
        vTaskDelay(pdMS_TO_TICKS(24));   // 16ms + margem
    }

    ESP_LOGI(TAG, "BH1750FVI sensor initialized successfully");
    return SENSOR_OK;
}

SensorStatus_t BH1750FVI_read_data(sensor_base_t* self, void* data) {
    if (self == NULL || data == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return SENSOR_ERROR;
    }

    BH1750FVI_sensor_t* bh1750 = (BH1750FVI_sensor_t*)self;
    BH1750FVI_data_t* sensor_data = (BH1750FVI_data_t*)data;

    uint16_t raw_value;
    esp_err_t err = BH1750FVI_read_raw(bh1750, &raw_value);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read sensor data");
        return SENSOR_ERROR;
    }

    sensor_data->raw_value = raw_value;
    
    // Converter para lux
    // Fórmula: lux = raw_value / 1.2 (para modo H-resolution)
    // Para H-resolution mode 2: lux = raw_value / 2.4
    if (bh1750->mode == BH1750_CONTINUOUS_H_RES2 || bh1750->mode == BH1750_ONE_TIME_H_RES2) {
        sensor_data->lux = raw_value / 2.4;
    } else if (bh1750->mode == BH1750_CONTINUOUS_L_RES || bh1750->mode == BH1750_ONE_TIME_L_RES) {
        sensor_data->lux = raw_value / 1.2;
    } else {
        sensor_data->lux = raw_value / 1.2;
    }

    ESP_LOGI(TAG, "Light level: %.2f lux (raw: %d)", sensor_data->lux, raw_value);
    
    return SENSOR_OK;
}

SensorStatus_t BH1750FVI_deinit(sensor_base_t* self) {
    if (self == NULL) {
        return SENSOR_ERROR;
    }

    BH1750FVI_sensor_t* bh1750 = (BH1750FVI_sensor_t*)self;

    // Power down o sensor
    BH1750FVI_write_command(bh1750, BH1750_POWER_DOWN);

    // Desinstalar driver I2C
    i2c_driver_delete(bh1750->i2c_port);

    // Liberar memória
    if (self->context != NULL) {
        free(self->context);
    }
    free(self);

    ESP_LOGI(TAG, "BH1750FVI sensor deinitialized");
    return SENSOR_OK;
}
