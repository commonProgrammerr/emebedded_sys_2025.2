#ifndef MAX7219_DRIVER_H
#define MAX7219_DRIVER_H

#include "main.h"
#include "string.h"

#define MAX7219_REG_NOOP 0x00
#define MAX7219_REG_DIGIT0 0x01
#define MAX7219_REG_DIGIT1 0x02
#define MAX7219_REG_DIGIT2 0x03
#define MAX7219_REG_DIGIT3 0x04
#define MAX7219_REG_DIGIT4 0x05
#define MAX7219_REG_DIGIT5 0x06
#define MAX7219_REG_DIGIT6 0x07
#define MAX7219_REG_DIGIT7 0x08
#define MAX7219_REG_DECODEMODE 0x09
#define MAX7219_REG_INTENSITY 0x0A
#define MAX7219_REG_SCANLIMIT 0x0B
#define MAX7219_REG_SHUTDOWN 0x0C
#define MAX7219_REG_DISPLAYTEST 0x0F

typedef void (*MAX7219_Tx_CpltCallback_t)(SPI_HandleTypeDef *hspi);

typedef struct MAX7219_driver
{
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;
    uint8_t screen_buffer[8];
    volatile uint8_t display_line;
    MAX7219_Tx_CpltCallback_t tx_cplt_callback;
} MAX7219_driver_t;


void MAX7219_Init(SPI_HandleTypeDef *hspi, GPIO_TypeDef *cs_port, uint16_t cs_pin, MAX7219_Tx_CpltCallback_t tx_cplt_callback);
void MAX7219_Write(uint16_t address, uint8_t data);
void MAX7219_TxCpltHandle(SPI_HandleTypeDef *hspi);
void MAX7219_UpdateScreen(uint8_t new_screen[8]);

static MAX7219_driver_t max7219;
#endif // MAX7219_DRIVER_H