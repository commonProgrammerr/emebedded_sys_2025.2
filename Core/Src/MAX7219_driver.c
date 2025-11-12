#include "MAX7219_driver.h"

MAX7219_driver_t max7219;

void MAX7219_Init(SPI_HandleTypeDef *hspi, GPIO_TypeDef *cs_port, uint16_t cs_pin, MAX7219_Tx_CpltCallback_t tx_cplt_callback)
{
    max7219.hspi = hspi;
    max7219.cs_port = cs_port;
    max7219.cs_pin = cs_pin;
    max7219.display_line = 0;
    max7219.tx_cplt_callback = tx_cplt_callback;

    // Disable display test
    HAL_SPI_Transmit_DMA(hspi, (uint8_t[]){MAX7219_REG_DISPLAYTEST, 0x00}, 2);

    // Enter shutdown mode to configure
    HAL_SPI_Transmit_DMA(hspi, (uint8_t[]){MAX7219_REG_SHUTDOWN, 0x00}, 2);

    // Set scan limit to 8 digits
    HAL_SPI_Transmit_DMA(hspi, (uint8_t[]){MAX7219_REG_SCANLIMIT, 0x07}, 2);

    // Disable decoding (matrix mode)
    HAL_SPI_Transmit_DMA(hspi, (uint8_t[]){MAX7219_REG_DECODEMODE, 0x00}, 2);

    // Set medium intensity
    HAL_SPI_Transmit_DMA(hspi, (uint8_t[]){MAX7219_REG_INTENSITY, 0x00}, 2);

    // Clear all digits
    MAX7219_UpdateScreen((uint8_t[]){0, 0, 0, 0, 0, 0, 0, 0});

    // Exit shutdown mode
    HAL_SPI_Transmit_DMA(hspi, (uint8_t[]){MAX7219_REG_SHUTDOWN, 0x01}, 2);
}
extern UART_HandleTypeDef huart2;

void MAX7219_TxCpltHandler(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == max7219.hspi->Instance)
    {
        // Lower the LOAD/CS pin
        HAL_GPIO_WritePin(max7219.cs_port, max7219.cs_pin, GPIO_PIN_RESET);
        // Raise the LOAD/CS pin
        HAL_GPIO_WritePin(max7219.cs_port, max7219.cs_pin, GPIO_PIN_SET);

        if (max7219.display_line < 8)
        {
            uint8_t data[] = {MAX7219_REG_DIGIT0 + max7219.display_line, max7219.screen_buffer[max7219.display_line++]};
            HAL_SPI_Transmit_DMA(max7219.hspi, data, 2);
        }
        else if (max7219.tx_cplt_callback != NULL) // Call user-defined callback if set
        {
            max7219.tx_cplt_callback(hspi);
        }
    }
}

void MAX7219_UpdateScreen(uint8_t new_screen[8])
{
    max7219.display_line = 0;
    max7219.screen_buffer = new_screen;
    HAL_SPI_Transmit_DMA(max7219.hspi, (uint8_t[]){MAX7219_REG_DIGIT0, max7219.screen_buffer[0]}, 2);
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    MAX7219_TxCpltHandler(hspi);
}
