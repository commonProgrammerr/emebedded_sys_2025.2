/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "PCF8591_driver.h"
#include "cmd_driver.h"
#include "MAX7219_driver.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef enum
{
  SYSTEM_STATE_1 = 1,
  SYSTEM_STATE_2 = 2,
  SYSTEM_STATE_3 = 3,
  SYSTEM_STATE_4 = 4,
  SYSTEM_STATE_5 = 5,
  SYSTEM_STATE_6 = 6,
  SYSTEM_STATE_7 = 7,
  SYSTEM_STATE_8 = 8,
} SystemState;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define I2C_INTERFACE hi2c3
#define I2C_INTERFACE_INSTANCE I2C3
#define TEMPERATURE_SCREEN { \
    0b00000000,              \
    0b01111110,              \
    0b01111110,              \
    0b00011000,              \
    0b00011000,              \
    0b00011000,              \
    0b00011000,              \
    0b00011000,              \
    0b00000000,              \
}
#define TENSION_SCREEN { \
    0b00000000,          \
    0b10000001,          \
    0b11000011,          \
    0b11000011,          \
    0b01100110,          \
    0b00111100,          \
    0b00011000,          \
    0b00000000,          \
}
#define LIGHT_SCREEN { \
    0b00000000,        \
    0b00110000,        \
    0b00110000,        \
    0b00110000,        \
    0b00110000,        \
    0b00111100,        \
    0b00111100,        \
    0b00000000,        \
}
#define PLUS_SCREEN { \
    0b00000000,       \
    0b00011000,       \
    0b00011000,       \
    0b01111110,       \
    0b01111110,       \
    0b00011000,       \
    0b00011000,       \
    0b00000000,       \
}
#define MINUS_SCREEN { \
    0b00000000,        \
    0b00000000,        \
    0b00000000,        \
    0b00111100,        \
    0b00111100,        \
    0b00000000,        \
    0b00000000,        \
    0b00000000,        \
}
// Define the MAX7219 registers
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c3;
DMA_HandleTypeDef hdma_i2c3_rx;
DMA_HandleTypeDef hdma_i2c3_tx;

SPI_HandleTypeDef hspi1;
DMA_HandleTypeDef hdma_spi1_tx;

TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart2_rx;
DMA_HandleTypeDef hdma_usart2_tx;

/* USER CODE BEGIN PV */
volatile SystemState system_state = SYSTEM_STATE_1;
volatile SystemState last_state = SYSTEM_STATE_1;
volatile PCF8591_Channel channel_index = PCF8591_A0;

uint8_t channels[4];
uint8_t dac_value = 0;
char dac_str[3];

uint8_t timeout_counter = 0;
uint8_t updating_screen = 0;
uint8_t plus_screen[] = PLUS_SCREEN;
uint8_t minus_screen[] = MINUS_SCREEN;
uint8_t temperature_screen[] = TEMPERATURE_SCREEN;
uint8_t tension_screen[] = TENSION_SCREEN;
uint8_t light_screen[] = LIGHT_SCREEN;
uint8_t blank_screen[] = {0, 0, 0, 0, 0, 0, 0, 0};
uint8_t *display_buffer[] = {blank_screen, blank_screen};
uint8_t current_screen = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_SPI1_Init(void);
static void MX_TIM2_Init(void);
static void MX_I2C3_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// Critical section helpers for mutex-like behavior

// Safer state transitions
static void set_system_state(SystemState new_state)
{
#ifdef DEBUG
  char msg[50];
  snprintf(msg, sizeof(msg), "[debug] State changed from %d to %d\r\n", last_state, system_state);
  HAL_UART_Transmit_DMA(&huart2, (uint8_t *)msg, strlen(msg));
#endif
  last_state = system_state;
  system_state = new_state;
}

static SystemState get_system_state(void)
{
  SystemState state = system_state;
  return state;
}

void PCF8591_TxCpltCallback(I2C_HandleTypeDef *hi2c)
{
  // Transmission complete. Go to next state
  if (get_system_state() == SYSTEM_STATE_2)
    set_system_state(SYSTEM_STATE_3);
  else if (get_system_state() == SYSTEM_STATE_5)
    set_system_state(SYSTEM_STATE_4);
  else if (get_system_state() == SYSTEM_STATE_6)
    set_system_state(SYSTEM_STATE_7);
}

void PCF8591_RxCpltCallback(I2C_HandleTypeDef *hi2c)
{
  if (get_system_state() == SYSTEM_STATE_3)
    set_system_state(SYSTEM_STATE_4); // go to next state
  else if (get_system_state() == SYSTEM_STATE_7)
  {
    uint8_t i = PCF8591_get_channel_index();
    display_buffer[0] = (channels[i] > (pcf8591.analog_data)[i]) ? minus_screen : plus_screen;
    channels[i] = (pcf8591.analog_data)[i];
    set_system_state(SYSTEM_STATE_8); // go to next state
  }
}

void MAX7219_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
  if (get_system_state() == SYSTEM_STATE_8) // Transmission complete
  {
    updating_screen = 0;
    set_system_state(SYSTEM_STATE_1); // back to initial state
  }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2 && get_system_state() == SYSTEM_STATE_4) // Transmission complete
    set_system_state(SYSTEM_STATE_1);                                    // back to initial state
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM2)
  {
    if (get_system_state() == SYSTEM_STATE_1)
      set_system_state(SYSTEM_STATE_8); // Move to screen update state
  }
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi->Instance == SPI1)
    MAX7219_TxCpltCallback(hspi);
}

void process_uart_commands(char *cmd, uint16_t size)
{
  uint8_t err = 0;
  // Process the complete command (cmd_buffer is now stable)
  if (strncmp(cmd, "Read_AIN", 8) == 0)
  {
    // Handle read command
    char channel = cmd[8]; // Get channel number
    if (channel >= '0' && channel <= '3')
    {
      channel_index = (PCF8591_Channel)(channel - '0');
      set_system_state(SYSTEM_STATE_2);
    }
    else
      err = 1;
  }
  else if (strncmp(cmd, "Set_DAC_", 8) == 0)
  {
    // Handle DAC command
    int dac_arg = atoi(&cmd[8]);
    if (dac_arg > 0 && dac_arg <= 255)
    {
      dac_value = (uint8_t)dac_arg;
      set_system_state(SYSTEM_STATE_5);
    }
    else
      err = 1;
  }
  else
  {
    // Handle display commands
    if (strncmp(cmd, "Temp", 4) == 0)
    {
      display_buffer[1] = temperature_screen;
      channel_index = 1;
    }
    else if (strncmp(cmd, "Volt", 4) == 0)
    {
      display_buffer[1] = tension_screen;
      channel_index = 3;
    }
    else if (strncmp(cmd, "LDR", 3) == 0)
    {
      display_buffer[1] = light_screen;
      channel_index = 0;
    }
    else
      err = 1;

    if (err == 0)
      set_system_state(SYSTEM_STATE_6);
  }

  if (err == 1)
  {
    const char error_msg[200];
    sprintf(error_msg, "Unknown command: %s\n\r", cmd);
    HAL_UART_Transmit_DMA(&huart2, (uint8_t *)error_msg, strlen(error_msg));
  }
}

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_SPI1_Init();
  MX_TIM2_Init();
  MX_I2C3_Init();
  /* USER CODE BEGIN 2 */
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  char tx_buff[100];
  set_system_state(SYSTEM_STATE_1);
  PCF8591_Init(&I2C_INTERFACE, NULL, PCF8591_TxCpltCallback, PCF8591_RxCpltCallback);
  MAX7219_Init(&hspi1, GPIOA, GPIO_PIN_4, MAX7219_TxCpltCallback);
  cmd_driver_init(&huart2, process_uart_commands);
  HAL_TIM_Base_Start_IT(&htim2); // Start timer for periodic tasks
  
  SystemState local_state = get_system_state();
  while (1)
  {
    switch (get_system_state())
    {
    case SYSTEM_STATE_1:
      cmd_tick();
      break;
    case SYSTEM_STATE_2:
    case SYSTEM_STATE_6:
      PCF8591_set_channel_index(channel_index);
      __WFI(); // Wait for interrupt
      break;
    case SYSTEM_STATE_3:
    case SYSTEM_STATE_7:
      PCF8591_read_analog_channel();
      __WFI(); // Wait for interrupt
      break;
    case SYSTEM_STATE_5:
      PCF8591_write_dac(dac_value);
      __WFI(); // Wait for interrupt
      break;
    case SYSTEM_STATE_4:
      if (last_state == SYSTEM_STATE_3)
        sprintf(tx_buff, "AIN%d: %d\n\r", channel_index, channels[channel_index]);
      else if (last_state == SYSTEM_STATE_5)
        sprintf(tx_buff, "Valor do DAC: %d\n\r", dac_value);
      else
        sprintf(tx_buff, "Error de execução. Transição desconhecida\n\r");

      HAL_UART_Transmit_DMA(&huart2, (uint8_t *)tx_buff, strlen(tx_buff));
      break;
    case SYSTEM_STATE_8:
      updating_screen = 1;
      current_screen = (current_screen + 1) % 2;

      MAX7219_UpdateScreen(display_buffer[current_screen]);
      while (updating_screen)
        __WFI(); // Wait for interrupt
      break;
    default:
      break;
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
   */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
 * @brief I2C3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C3_Init(void)
{

  /* USER CODE BEGIN I2C3_Init 0 */

  /* USER CODE END I2C3_Init 0 */

  /* USER CODE BEGIN I2C3_Init 1 */

  /* USER CODE END I2C3_Init 1 */
  hi2c3.Instance = I2C3;
  hi2c3.Init.Timing = 0x10D19CE4;
  hi2c3.Init.OwnAddress1 = 0;
  hi2c3.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c3.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c3.Init.OwnAddress2 = 0;
  hi2c3.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c3.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c3.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c3) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
   */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c3, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
   */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c3, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C3_Init 2 */

  /* USER CODE END I2C3_Init 2 */
}

/**
 * @brief SPI1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */
}

/**
 * @brief TIM2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 7999;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 5000;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
}

/**
 * @brief USART2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */
}

/**
 * Enable DMA controller clock
 */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);
  /* DMA1_Channel3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);
  /* DMA1_Channel6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel6_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel6_IRQn);
  /* DMA1_Channel7_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel7_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel7_IRQn);
  /* DMA2_Channel4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Channel4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Channel4_IRQn);
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(I2C_VCC_GPIO_Port, I2C_VCC_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, I2C_GND_Pin | SPI_GND_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(SPI_VCC_GPIO_Port, SPI_VCC_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : I2C_VCC_Pin */
  GPIO_InitStruct.Pin = I2C_VCC_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(I2C_VCC_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : CS_Pin */
  GPIO_InitStruct.Pin = CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(CS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : I2C_GND_Pin SPI_GND_Pin */
  GPIO_InitStruct.Pin = I2C_GND_Pin | SPI_GND_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : SPI_VCC_Pin */
  GPIO_InitStruct.Pin = SPI_VCC_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(SPI_VCC_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
