#if !defined(PCF8591_DRIVER_H)
#define PCF8591_DRIVER_H

/* TODO: Add comprehensive documentation for driver API, especially callback
 * behavior and expected state machine integration. */
#include "main.h"

#define PCF8591_ADDRESS (0x48 << 1) // Shifted PCF8591 I2C address

typedef enum
{
  PCF8591_A0 = 0,
  PCF8591_A1 = 1,
  PCF8591_A2 = 2,
  PCF8591_A3 = 3
} PCF8591_Channel;

// callback handler
typedef void (*PCF8591_TickCallback_t)(I2C_HandleTypeDef *hi2c);
typedef struct PCF8591_driver
{
  I2C_HandleTypeDef *hi2c;
  PCF8591_TickCallback_t tick_callback;
  PCF8591_TickCallback_t tx_cplt_callback;
  PCF8591_TickCallback_t rx_cplt_callback;
  uint8_t dac_value;       // Current DAC output value
  uint16_t analog_data[4]; // Buffer to hold analog channel data
} PCF8591_driver_t;

void PCF8591_tick(I2C_HandleTypeDef *hi2c);
void PCF8591_tx_cplt_handler(I2C_HandleTypeDef *hi2c);
void PCF8591_rx_cplt_handler(I2C_HandleTypeDef *hi2c);
void PCF8591_write_dac(uint8_t value);
void PCF8591_set_channel_index(uint8_t channel_index);
uint8_t PCF8591_get_channel_index(void);
void PCF8591_read_analog_channel(void);

// Inicialização
void PCF8591_Init(
    I2C_HandleTypeDef *hi2c,
    PCF8591_TickCallback_t tick_callback,
    PCF8591_TickCallback_t tx_cplt_callback,
    PCF8591_TickCallback_t rx_cplt_callback);

extern PCF8591_driver_t pcf8591;
#endif // PCF8591_DRIVER_H
