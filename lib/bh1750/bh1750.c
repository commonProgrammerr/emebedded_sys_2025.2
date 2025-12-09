#include "bh1750.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

static const char *TAG = "BH1750";

// Função auxiliar para enviar comando (com tratamento de erro melhorado)
static esp_err_t bh1750_send_cmd(bh1750_t *bh1750, uint8_t cmd)
{
  if (bh1750 == NULL || bh1750->dev_handle == NULL)
  {
    ESP_LOGE(TAG, "Handle inválido");
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t ret = i2c_master_transmit(bh1750->dev_handle, &cmd, 1, 500);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Falha ao enviar comando 0x%02x: %s", cmd, esp_err_to_name(ret));
  }
  return ret;
}

// Verifica se o dispositivo está presente
static esp_err_t bh1750_probe(bh1750_t *bh1750)
{
  if (bh1750 == NULL)
  {
    return ESP_ERR_INVALID_ARG;
  }

  // Tenta enviar um comando simples (Power On)
  esp_err_t ret = bh1750_send_cmd(bh1750, BH1750_POWER_ON);
  if (ret == ESP_OK)
  {
    ESP_LOGD(TAG, "Dispositivo respondeu no endereço 0x%02x", bh1750->address);
    return ESP_OK;
  }

  ESP_LOGE(TAG, "Dispositivo NÃO respondeu no endereço 0x%02x", bh1750->address);
  return ret;
}

// Inicializa o sensor
esp_err_t bh1750_init(i2c_master_bus_handle_t i2c_bus_handle, uint8_t address, bh1750_t *bh1750)
{
  if (bh1750 == NULL)
  {
    ESP_LOGE(TAG, "Ponteiro bh1750 é nulo");
    return ESP_ERR_INVALID_ARG;
  }

  // Configura o dispositivo I2C
  i2c_device_config_t dev_cfg = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = address,
      .scl_speed_hz = 50000, // Começa com 50 kHz (mais lento, mais confiável)
  };

  esp_err_t ret = i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &(bh1750->dev_handle));
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Falha ao adicionar dispositivo I2C (endereço 0x%02x): %s",
             address, esp_err_to_name(ret));
    return ret;
  }

  bh1750->address = address;
  bh1750->mtreg = BH1750_DEFAULT_MTREG;
  bh1750->mode = 0;

  // Pequeno delay para estabilização
  vTaskDelay(pdMS_TO_TICKS(10));

  // 1. Tenta fazer power on
  ret = bh1750_send_cmd(bh1750, BH1750_POWER_ON);
  if (ret != ESP_OK)
  {
    ESP_LOGW(TAG, "Primeira tentativa de power on falhou, tentando novamente...");
    vTaskDelay(pdMS_TO_TICKS(10));
    ret = bh1750_send_cmd(bh1750, BH1750_POWER_ON);
    if (ret != ESP_OK)
    {
      ESP_LOGE(TAG, "Power on falhou após múltiplas tentativas");
      i2c_master_bus_rm_device(bh1750->dev_handle);
      bh1750->dev_handle = NULL;
      return ret;
    }
  }

  vTaskDelay(pdMS_TO_TICKS(10));

  // 2. Tenta reset
  ret = bh1750_send_cmd(bh1750, BH1750_RESET);
  if (ret != ESP_OK)
  {
    ESP_LOGW(TAG, "Reset falhou, continuando...");
  }

  vTaskDelay(pdMS_TO_TICKS(10));

  // 3. Configura MTreg padrão
  ret = bh1750_set_mtreg(bh1750, BH1750_DEFAULT_MTREG);
  if (ret != ESP_OK)
  {
    ESP_LOGW(TAG, "Configuração MTreg falhou, usando padrão...");
  }

  // 4. Verifica se o sensor realmente responde
  ret = bh1750_probe(bh1750);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Sensor não responde após inicialização");
    i2c_master_bus_rm_device(bh1750->dev_handle);
    bh1750->dev_handle = NULL;
    return ret;
  }

  ESP_LOGI(TAG, "Sensor BH1750 inicializado com sucesso (endereço 0x%02x)", address);
  return ESP_OK;
}

// Configura modo de medição
esp_err_t bh1750_set_measurement_mode(bh1750_t *bh1750, uint8_t mode, uint32_t wait_ms)
{
  if (bh1750 == NULL)
  {
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t ret = bh1750_send_cmd(bh1750, mode);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Falha ao configurar modo 0x%02x", mode);
    return ret;
  }

  bh1750->mode = mode;

  if (wait_ms == 0)
  {
    if (mode == BH1750_CONT_L_RES || mode == BH1750_ONE_L_RES)
    {
      wait_ms = 24;
    }
    else
    {
      wait_ms = 180;
    }
  }

  vTaskDelay(pdMS_TO_TICKS(wait_ms));

  return ESP_OK;
}

// Configura o MTreg
esp_err_t bh1750_set_mtreg(bh1750_t *bh1750, uint8_t mtreg)
{
  if (bh1750 == NULL)
  {
    return ESP_ERR_INVALID_ARG;
  }

  if (mtreg < 31 || mtreg > 254)
  {
    ESP_LOGE(TAG, "MTreg fora do intervalo (31-254)");
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t high_cmd = 0x40 | (mtreg >> 5);
  uint8_t low_cmd = 0x60 | (mtreg & 0x1F);

  esp_err_t ret = bh1750_send_cmd(bh1750, high_cmd);
  if (ret != ESP_OK)
  {
    return ret;
  }

  ret = bh1750_send_cmd(bh1750, low_cmd);
  if (ret != ESP_OK)
  {
    return ret;
  }

  bh1750->mtreg = mtreg;
  ESP_LOGD(TAG, "MTreg alterado para %d", mtreg);
  return ESP_OK;
}

// Lê a iluminância em lux
esp_err_t bh1750_read_lux(bh1750_t *bh1750, float *lux)
{
  if (bh1750 == NULL || lux == NULL)
  {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t buffer[2] = {0};
  esp_err_t ret = i2c_master_receive(bh1750->dev_handle, buffer, 2, 100 / portTICK_PERIOD_MS);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Falha ao ler dados do sensor: %s", esp_err_to_name(ret));
    return ret;
  }

  uint16_t raw_value = (buffer[0] << 8) | buffer[1];
  if (raw_value == 0xFFFF)
  {
    ESP_LOGE(TAG, "Valor de leitura inválido (overflow)");
    return ESP_ERR_INVALID_RESPONSE;
  }

  // Cálculo correto baseado no datasheet
  float factor = 1.0;
  if (bh1750->mode == BH1750_CONT_H_RES2 || bh1750->mode == BH1750_ONE_H_RES2)
  {
    factor = 0.5; // Modo 0.5 lx
  }
  else if (bh1750->mode == BH1750_CONT_L_RES || bh1750->mode == BH1750_ONE_L_RES)
  {
    factor = 4.0; // Modo 4 lx
  }

  *lux = (raw_value * factor) / 1.2 * (69.0 / bh1750->mtreg);

  ESP_LOGD(TAG, "Raw: %d, MTreg: %d, Mode: 0x%02x, Lux: %.2f",
           raw_value, bh1750->mtreg, bh1750->mode, *lux);
  return ESP_OK;
}

// Funções auxiliares
esp_err_t bh1750_reset(bh1750_t *bh1750)
{
  return bh1750_send_cmd(bh1750, BH1750_RESET);
}

esp_err_t bh1750_power_down(bh1750_t *bh1750)
{
  return bh1750_send_cmd(bh1750, BH1750_POWER_DOWN);
}

esp_err_t bh1750_power_on(bh1750_t *bh1750)
{
  return bh1750_send_cmd(bh1750, BH1750_POWER_ON);
}

// Desinicializa o sensor
esp_err_t bh1750_deinit(bh1750_t *bh1750)
{
  if (bh1750 == NULL || bh1750->dev_handle == NULL)
  {
    return ESP_ERR_INVALID_ARG;
  }

  // Tenta colocar em power down
  bh1750_send_cmd(bh1750, BH1750_POWER_DOWN);

  // Remove o dispositivo
  esp_err_t ret = i2c_master_bus_rm_device(bh1750->dev_handle);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Falha ao remover dispositivo I2C: %s", esp_err_to_name(ret));
    return ret;
  }

  bh1750->dev_handle = NULL;
  ESP_LOGI(TAG, "Sensor BH1750 desinicializado");
  return ESP_OK;
}