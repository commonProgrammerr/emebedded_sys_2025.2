#include "PCF8591_driver.h"

PCF8591_driver_t pcf8591;
uint8_t pcf8591_config = 0x40;
uint8_t pcf8591_channel_index = 0;

void PCF8591_tick(I2C_HandleTypeDef *hi2c)
{
  if (hi2c->Instance == pcf8591.hi2c->Instance)
    pcf8591.analog_data[pcf8591_channel_index % 4] &= 0x00FF; // Clear high byte.

  if (pcf8591.tick_callback != NULL)
    pcf8591.tick_callback(hi2c);
}
void PCF8591_rx_cplt_handler(I2C_HandleTypeDef *hi2c)
{
  PCF8591_tick(hi2c); // Call the tick handler
  if (hi2c->Instance == pcf8591.hi2c->Instance && pcf8591.rx_cplt_callback != NULL)
    pcf8591.rx_cplt_callback(hi2c);
}
void PCF8591_tx_cplt_handler(I2C_HandleTypeDef *hi2c)
{
  PCF8591_tick(hi2c); // Call the tick handler
  if (hi2c->Instance == pcf8591.hi2c->Instance && pcf8591.tx_cplt_callback != NULL)
    pcf8591.tx_cplt_callback(hi2c);
}

void PCF8591_set_channel_index(uint8_t channel_index)
{
  if (channel_index > 3)
    return; // Invalid channel index

  uint8_t config_byte = pcf8591_config | (channel_index & 0x03); // Select the channel (A0, A1, A2, A3)
  uint8_t analog_data[2];
  // Send configuration byte to select the ADC channel
  if (HAL_OK == HAL_I2C_Master_Transmit_IT(pcf8591.hi2c, PCF8591_ADDRESS, &config_byte, 1))
    pcf8591_channel_index = channel_index;
}

uint8_t PCF8591_get_channel_index()
{
  return pcf8591_channel_index;
}

void PCF8591_read_analog_channel()
{
  uint8_t config_byte = pcf8591_config | (pcf8591_channel_index & 0x03); // Select the channel (A0, A1, A2, A3)
  uint8_t *analog_data = (uint8_t *)&pcf8591.analog_data[pcf8591_channel_index % 4];

  // Read two bytes: first byte is a dummy, second byte is the actual analog value
  HAL_I2C_Master_Receive_IT(pcf8591.hi2c, PCF8591_ADDRESS, analog_data, 2);
}

void PCF8591_write_dac(uint8_t value)
{
  uint8_t config_byte = pcf8591_config | 0x40; // Enable analog output
  uint8_t send_data[2] = {config_byte, value};

  // Send configuration byte to select the ADC channel
  if (HAL_OK == HAL_I2C_Master_Transmit_IT(&pcf8591.hi2c, PCF8591_ADDRESS, send_data, 2))
    pcf8591.dac_value = value; // update current DAC value
  __NOP();                     // Small delay to allow ADC to settle
}

// Inicialização
void PCF8591_Init(I2C_HandleTypeDef *hspi, PCF8591_TickCallback_t tick_callback, PCF8591_TickCallback_t tx_cplt_callback, PCF8591_TickCallback_t rx_cplt_callback)
{
  pcf8591.hi2c = hspi;
  pcf8591.tick_callback = tick_callback;
  pcf8591.tx_cplt_callback = tx_cplt_callback;
  pcf8591.rx_cplt_callback = rx_cplt_callback;
  pcf8591.dac_value = 0;
  for (int i = 0; i < 4; i++)
    pcf8591.analog_data[i] = 0;
}

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
  PCF8591_tx_cplt_handler(hi2c); // Call the transmission complete handler
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
  PCF8591_rx_cplt_handler(hi2c); // Call the reception complete handler
}
