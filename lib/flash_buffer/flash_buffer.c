/**
 * @file sensor_flash_buffer.c
 * @brief Implementação do buffer circular não volátil na flash
 */

#include "flash_buffer.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "flash_buffer";

// Chaves NVS para metadados
#define KEY_WRITE_IDX "w_idx"
#define KEY_COUNT "count"
#define KEY_SAMPLE_FMT "s_%lx" // Formato para chave de amostra

esp_err_t flash_buffer_system_init(void)
{
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // NVS partition foi truncada e precisa ser apagada
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Falha ao inicializar NVS: %s", esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGI(TAG, "NVS inicializado com sucesso");
    }

    return ret;
}

/* Global buffer pointer (optional usage) */
static flash_buffer_t* g_flash_buffer = NULL;

void flash_buffer_set_global(flash_buffer_t* buffer)
{
    g_flash_buffer = buffer;
}

flash_buffer_t* flash_buffer_get_global(void)
{
    return g_flash_buffer;
}

flash_buffer_t *flash_buffer_init(const char *namespace, size_t sample_size, uint32_t max_samples)
{
    if (!namespace || max_samples == 0 || (max_samples*sample_size) > (16 * 1024))
    {
        ESP_LOGE(TAG, "Parâmetros inválidos");
        return NULL;
    }

    flash_buffer_t *buffer = calloc(1, sizeof(flash_buffer_t));
    if (!buffer)
    {
        ESP_LOGE(TAG, "Falha ao alocar memória para buffer");
        return NULL;
    }

    // Copia o namespace
    strncpy(buffer->namespace, namespace, sizeof(buffer->namespace) - 1);
    buffer->max_samples = max_samples;
    buffer->sample_size = sample_size;

    // Abre o namespace NVS
    esp_err_t err = nvs_open(buffer->namespace, NVS_READWRITE, &buffer->nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Erro ao abrir NVS namespace '%s': %s",
                 buffer->namespace, esp_err_to_name(err));
        free(buffer);
        return NULL;
    }

    // Recupera metadados (se existirem)
    nvs_get_u32(buffer->nvs_handle, KEY_WRITE_IDX, &buffer->write_index);
    nvs_get_u32(buffer->nvs_handle, KEY_COUNT, &buffer->sample_count);

    ESP_LOGI(TAG, "Buffer '%s' inicializado: %lu/%lu amostras",
             buffer->namespace, buffer->sample_count, buffer->max_samples);

    return buffer;
}

esp_err_t flash_buffer_write(flash_buffer_t *buffer, const void *sample)
{
    if (!buffer || !sample)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Gera chave para esta amostra
    char key[16];
    snprintf(key, sizeof(key), KEY_SAMPLE_FMT, buffer->write_index);

    // Salva a amostra
    esp_err_t err = nvs_set_blob(buffer->nvs_handle, key, sample, buffer->sample_size);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Erro ao salvar amostra: %s", esp_err_to_name(err));
        return err;
    }

    // Atualiza índices
    buffer->write_index = (buffer->write_index + 1) % buffer->max_samples;
    if (buffer->sample_count < buffer->max_samples)
    {
        buffer->sample_count++;
    }

    // Salva metadados
    nvs_set_u32(buffer->nvs_handle, KEY_WRITE_IDX, buffer->write_index);
    nvs_set_u32(buffer->nvs_handle, KEY_COUNT, buffer->sample_count);

    // Commit (escreve na flash)
    err = nvs_commit(buffer->nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Erro ao commit NVS: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGD(TAG, "Amostra salva: idx=%lu, count=%lu",
             buffer->write_index, buffer->sample_count);

    return ESP_OK;
}

uint32_t flash_buffer_read(flash_buffer_t *buffer, void *samples, uint32_t count)
{
    if (!buffer || !samples || count == 0)
    {
        return 0;
    }

    uint32_t read_count = (count < buffer->sample_count) ? count : buffer->sample_count;
    uint8_t *sample_ptr = (uint8_t *)samples;
    uint32_t successfully_read = 0;

    // Lê as amostras mais recentes
    for (uint32_t i = 0; i < read_count; i++)
    {
        // Calcula índice (lê de trás para frente - mais recentes primeiro)
        int32_t idx = (int32_t)buffer->write_index - 1 - (int32_t)i;
        if (idx < 0)
        {
            idx += buffer->max_samples;
        }

        char key[16];
        snprintf(key, sizeof(key), KEY_SAMPLE_FMT, (uint32_t)idx);

        size_t size = buffer->sample_size;
        void *dest = sample_ptr + (successfully_read * buffer->sample_size);
        
        // Inicializa com zeros antes de ler
        memset(dest, 0, buffer->sample_size);
        
        esp_err_t err = nvs_get_blob(buffer->nvs_handle, key, dest, &size);

        if (err != ESP_OK)
        {
            if (err != ESP_ERR_NVS_NOT_FOUND)
            {
                ESP_LOGW(TAG, "Erro ao ler amostra idx=%d: %s", idx, esp_err_to_name(err));
            }
            // Pula amostras não encontradas ou com erro
            continue;
        }
        
        // Verifica se o tamanho lido está correto
        if (size != buffer->sample_size)
        {
            ESP_LOGW(TAG, "Tamanho incorreto na amostra idx=%d: esperado=%zu, lido=%zu",
                     idx, buffer->sample_size, size);
            continue;
        }
        
        successfully_read++;
    }

    return successfully_read;
}

bool flash_buffer_read_last(flash_buffer_t *buffer, void *sample)
{
    if (!buffer || !sample || buffer->sample_count == 0)
    {
        return false;
    }

    // Última amostra está em write_index - 1
    int32_t idx = (int32_t)buffer->write_index - 1;
    if (idx < 0)
    {
        idx = buffer->max_samples - 1;
    }

    char key[16];
    snprintf(key, sizeof(key), KEY_SAMPLE_FMT, idx);

    size_t size = buffer->sample_size;
    esp_err_t err = nvs_get_blob(buffer->nvs_handle, key, sample, &size);

    return (err == ESP_OK);
}

uint32_t flash_buffer_get_count(const flash_buffer_t *buffer)
{
    return buffer ? buffer->sample_count : 0;
}

bool flash_buffer_is_full(const flash_buffer_t *buffer)
{
    return buffer ? (buffer->sample_count >= buffer->max_samples) : false;
}

esp_err_t flash_buffer_clear(flash_buffer_t *buffer)
{
    if (!buffer)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Apaga todos os dados deste namespace
    esp_err_t err = nvs_erase_all(buffer->nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Erro ao limpar buffer: %s", esp_err_to_name(err));
        return err;
    }

    // Reset dos contadores
    buffer->write_index = 0;
    buffer->sample_count = 0;

    err = nvs_commit(buffer->nvs_handle);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "Buffer '%s' limpo", buffer->namespace);
    }

    return err;
}

void flash_buffer_deinit(flash_buffer_t *buffer)
{
    if (buffer)
    {
        nvs_close(buffer->nvs_handle);
        ESP_LOGI(TAG, "Buffer '%s' fechado", buffer->namespace);
        free(buffer);
    }
}
