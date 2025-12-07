#include "KY-037_sensor.h"
#include <stdlib.h>
#include "esp_log.h"

static const char *TAG = "KY037";

void KY037_setup_adc(sensor_base_t* self) {
    KY037_sensor_t* ky037 = (KY037_sensor_t*)self;

    // Configuração da unidade ADC
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,  // ou ADC_UNIT_2
    };
    esp_err_t err = adc_oneshot_new_unit(&init_config, &(ky037->adc_handle));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADC unit");
        return;
    }

    // Configuração do canal
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,  // Resolução de 12 bits (0-4095)
        .atten = ADC_ATTEN_DB_11,     // Atenuação para ler até ~3.3V
    };
    err = adc_oneshot_config_channel(ky037->adc_handle, ky037->channel, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC channel");
        return;
    }

    ESP_LOGI(TAG, "ADC initialized successfully");
}

static SensorStatus_t KY037_init_wrapper(sensor_base_t* self) {
    KY037_sensor_t* ky037 = (KY037_sensor_t*)self;
    adc_channel_t channel = *((adc_channel_t*)self->context);

    return KY037_init(self, channel);
}

SensorStatus_t KY037_init(sensor_base_t* self, adc_channel_t channel) {
    if(self == NULL) {
        self = (sensor_base_t*)malloc(sizeof(KY037_sensor_t));
        if(self == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for sensor");
            return SENSOR_ERROR;
        }
    }

    KY037_sensor_t* ky037 = (KY037_sensor_t*)self;
    ky037->channel = channel;

    self->init = KY037_init_wrapper;
    self->read_data = KY037_read_data;
    self->deinit = KY037_deinit;

    if(self->context == NULL) {
        self->context = malloc(sizeof(adc_channel_t));
        if(self->context == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for context");
            free(self);
            return SENSOR_ERROR;
        }
        *((adc_channel_t*)self->context) = channel;
    }

    KY037_setup_adc(self);
    
    ESP_LOGI(TAG, "KY037 sensor initialized successfully on channel %d", channel);
    return SENSOR_OK;
}

SensorStatus_t KY037_read_data(sensor_base_t* self, void* data) {
    if (self == NULL || data == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return SENSOR_ERROR;
    }

    KY037_sensor_t* ky037 = (KY037_sensor_t*)self;
    uint32_t* adc_reading = (uint32_t*)data;

    esp_err_t err = adc_oneshot_read(ky037->adc_handle, ky037->channel, (int*)adc_reading);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read ADC data");
        return SENSOR_ERROR;
    }

    ESP_LOGI(TAG, "Sound level: %lu (raw ADC value)", *adc_reading);
    return SENSOR_OK;
}

SensorStatus_t KY037_deinit(sensor_base_t* self) {
    if (self == NULL) {
        return SENSOR_ERROR;
    }

    KY037_sensor_t* ky037 = (KY037_sensor_t*)self;

    // Deletar handle ADC
    adc_oneshot_del_unit(ky037->adc_handle);

    // Liberar memória
    if (self->context != NULL) {
        free(self->context);
    }
    free(self);

    ESP_LOGI(TAG, "KY037 sensor deinitialized");
    return SENSOR_OK;
}
