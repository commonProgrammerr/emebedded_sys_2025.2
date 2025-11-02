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
  PCF8591_A0 = 0,
  PCF8591_A1 = 1,
  PCF8591_A2 = 2,
  PCF8591_A3 = 3
} PCF8591_Channel;

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
#define PCF8591_ADDRESS (0x48 << 1) // Shifted PCF8591 I2C address
#define I2C_INTERFACE hi2c1
#define CMD_BUFFER_SIZE 50
#define TEMPERATURE_SCREEN { \
    0b00000000,              \
    0b01111110,              \
    0b00010000,              \
    0b00010000,              \
    0b00010000,              \
    0b00010000,              \
    0b0000000,               \
    0b00000000,              \
}
#define TENSION_SCREEN { \
    0b00000000,          \
    0b01000100,          \
    0b01000100,          \
    0b01000100,          \
    0b00101000,          \
    0b00010000,          \
    0b00000000,          \
    0b00000000,          \
}
#define LIGHT_SCREEN { \
    0b00000000,        \
    0b01000000,        \
    0b01000000,        \
    0b01000000,        \
    0b01000000,        \
    0b01111100,        \
    0b00000000,        \
    0b00000000,        \
}
#define PLUS_SCREEN { \
    0b00000000,       \
    0b00000000,       \
    0b00010000,       \
    0b00010000,       \
    0b01111100,       \
    0b00010000,       \
    0b00010000,       \
    0b00000000,       \
}
#define MINUS_SCREEN { \
    0b00000000,        \
    0b00000000,        \
    0b00000000,        \
    0b00000000,        \
    0b01111110,        \
    0b00000000,        \
    0b00000000,        \
    0b00000000,        \
}
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;
DMA_HandleTypeDef hdma_i2c1_rx;
DMA_HandleTypeDef hdma_i2c1_tx;

SPI_HandleTypeDef hspi1;
DMA_HandleTypeDef hdma_spi1_tx;

TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart2_rx;
DMA_HandleTypeDef hdma_usart2_tx;

/* USER CODE BEGIN PV */
volatile SystemState system_state = SYSTEM_STATE_1;
volatile PCF8591_Channel channel_index = PCF8591_A0;
uint8_t channels[4];
uint8_t current_command = 0;
uint8_t dac_value = 0;
char dac_str[3];
char cmd_buffer[CMD_BUFFER_SIZE];
volatile uint8_t cmd_index = 0;
volatile uint8_t cmd_ready = 0;
uint8_t rx_char;

uint8_t timeout_counter = 0;

uint8_t plus_screen[] = PLUS_SCREEN;
uint8_t minus_screen[] = MINUS_SCREEN;
uint8_t temperature_screen[] = TEMPERATURE_SCREEN;
uint8_t tension_screen[] = TENSION_SCREEN;
uint8_t light_screen[] = LIGHT_SCREEN;

uint8_t *display_buffer = NULL;
uint8_t *screens[3][3] = {
    {plus_screen, minus_screen, temperature_screen},
    {plus_screen, minus_screen, tension_screen},
    {plus_screen, minus_screen, light_screen},
};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI1_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// Critical section helpers for mutex-like behavior
static inline void enter_critical(void)
{
  __disable_irq();
}

static inline void exit_critical(void)
{
  __enable_irq();
}

// Safer state transitions
static void set_system_state(SystemState new_state)
{
  enter_critical();
  system_state = new_state;
  exit_critical();
}

static SystemState get_system_state(void)
{
  enter_critical();
  SystemState state = system_state;
  exit_critical();
  return state;
}

void PCF8591_UpdateAnalogChannelData(void)
{
  uint8_t config_byte = 0x40 | (channel_index & 0x03); // Select the channel (A0, A1, A2, A3)
  uint8_t analog_data[2];
  // Send configuration byte to select the ADC channel
  HAL_I2C_Master_Transmit_IT(&I2C_INTERFACE, PCF8591_ADDRESS, &config_byte, 1);
  HAL_Delay(1); // Small delay to allow ADC to settle

  // wait transmission end with low-power idle
  while (get_system_state() == SYSTEM_STATE_2)
  {
    __WFI(); // Wait for interrupt - saves power
  }

  // Read two bytes: first byte is a dummy, second byte is the actual analog value
  HAL_I2C_Master_Receive_IT(&I2C_INTERFACE, PCF8591_ADDRESS, analog_data, 2);

  // wait receive end with low-power idle
  while (get_system_state() == SYSTEM_STATE_3)
  {
    __WFI(); // Wait for interrupt - saves power
  }

  // Save the second byte which contains the valid ADC reading
  channels[channel_index] = analog_data[1];
}

void PCF8591_SetAnalogOutput(uint8_t dac_output)
{
  uint8_t config_byte = 0b01000000; // Enable analog output
  uint8_t send_data[2] = {
      config_byte,
      dac_output};
  // Send configuration byte to select the ADC channel
  HAL_I2C_Master_Transmit_IT(&I2C_INTERFACE, PCF8591_ADDRESS, send_data, 2);
  HAL_Delay(1); // Small delay to allow ADC to settle

  // wait transmission end with low-power idle
  while (get_system_state() == SYSTEM_STATE_6)
  {
    __WFI(); // Wait for interrupt - saves power
  }
}

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
  if (hi2c->Instance == I2C1)
  {
    // Transmission complete. Go to next state - use helper for atomic state change
    if (get_system_state() == SYSTEM_STATE_2)
      set_system_state(SYSTEM_STATE_3);
    else if (get_system_state() == SYSTEM_STATE_6)
      set_system_state(SYSTEM_STATE_4);
  }
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
  if (hi2c->Instance == I2C1) // Reception complete
  {
    if (get_system_state() == SYSTEM_STATE_3)
      set_system_state(SYSTEM_STATE_4); // go to next state
  }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2) // Transmission complete
  {
    if (get_system_state() == SYSTEM_STATE_4)
    {
      enter_critical();
      current_command = 0; // reset command flag
      exit_critical();
      set_system_state(SYSTEM_STATE_1); // back to initial state
    }
  }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2)
  {
    // Check if we received the terminator character
    if ((rx_char == '\n' || rx_char == '\r' || rx_char == ';') && cmd_index > 4) // Your terminator
    {
      cmd_buffer[cmd_index] = '\0'; // Null terminate
      cmd_ready = 1;                // Signal that command is ready
      cmd_index = 0;                // Reset for next command
    }
    else if (cmd_index < CMD_BUFFER_SIZE - 1)
    {
      cmd_buffer[cmd_index++] = rx_char;
    }

    // Continue receiving next character
    HAL_UART_Receive_IT(&huart2, &rx_char, 1);
  }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM2)
  {
    if (get_system_state() == SYSTEM_STATE_1)
      set_system_state(SYSTEM_STATE_8); // Move to screen update state
    else if (get_system_state() == SYSTEM_STATE_8)
    {
      timeout_counter++;
      if (timeout_counter >= 2)
      { // e.g., 2 * 500ms = 1 second timeout
        timeout_counter = 0;
        set_system_state(SYSTEM_STATE_1); // Return to idle state on timeout
      }
    }
  }
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi->Instance == SPI1) // Transmission complete
  {
    if (get_system_state() == SYSTEM_STATE_8)
    {
      set_system_state(SYSTEM_STATE_1); // back to initial state
    }
  }
}

void process_uart_commands(void)
{
  // Read cmd_ready atomically
  uint8_t ready;
  enter_critical();
  ready = cmd_ready;
  exit_critical();

  if (ready)
  {
    // Process the complete command (cmd_buffer is now stable)
    if (strncmp(cmd_buffer, "Read_AIN", 8) == 0)
    {
      // Handle read command
      char channel = cmd_buffer[8]; // Get channel number
      if (channel >= '0' && channel <= '3')
      {
        enter_critical();
        current_command = 'r';
        channel_index = (PCF8591_Channel)(channel - '0');
        exit_critical();
        set_system_state(SYSTEM_STATE_2);
      }
      else
      {
        enter_critical();
        current_command = 0; // Invalid channel
        exit_critical();
      }
    }
    else if (strncmp(cmd_buffer, "Set_DAC_", 8) == 0)
    {
      // Handle DAC command
      enter_critical();
      dac_value = atoi(&cmd_buffer[8]);
      current_command = 'w';
      exit_critical();
      set_system_state(SYSTEM_STATE_6);
    }
    else
    {
      enter_critical();
      channel_index = 4;
      if (strncmp(cmd_buffer, "Temp", 4) == 0)
        channel_index = 1;
      else if (strncmp(cmd_buffer, "Volt", 4) == 0)
        channel_index = 3;
      else if (strncmp(cmd_buffer, "LDR", 3) == 0)
        channel_index = 0;
      exit_critical();
      if (channel_index != 4)
        set_system_state(SYSTEM_STATE_6);
      else
      {
        // Unknown command
        HAL_UART_Transmit(&huart2, (uint8_t *)"Unknown command\n\r", 17, HAL_MAX_DELAY);
      }
    }

    // Clear ready flag atomically
    enter_critical();
    cmd_ready = 0;
    exit_critical();
  }
  else
  {
    HAL_UART_Receive_IT(&huart2, &rx_char, 1);
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
  MX_I2C1_Init();
  MX_SPI1_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  char tx_buff[100];
  SystemState last_state = SYSTEM_STATE_1;
  while (1)
  {
    if (last_state != system_state)
    {
      last_state = system_state;
      snprintf(tx_buff, sizeof(tx_buff), "[debug] Current state: %d\n\r", system_state + 1);
      HAL_UART_Transmit(&huart2, (uint8_t *)tx_buff, strlen(tx_buff), HAL_MAX_DELAY);
      if (system_state == SYSTEM_STATE_1)
      {
        HAL_UART_Transmit(&huart2, (uint8_t *)"Enter command: \n\r", 43, HAL_MAX_DELAY);
      }
    }
    switch (system_state)
    {
    case SYSTEM_STATE_1:
      process_uart_commands();
      HAL_Delay(100); // Delay to ensure data is ready
    case SYSTEM_STATE_3:
    case SYSTEM_STATE_5:
      break;
    case SYSTEM_STATE_2:
      PCF8591_UpdateAnalogChannelData();
      HAL_Delay(10); // Small delay to ensure data is ready
      break;
    case SYSTEM_STATE_6:
      PCF8591_SetAnalogOutput(dac_value);
      HAL_Delay(10); // Small delay to ensure data is ready
    case SYSTEM_STATE_4:
      if (current_command == 'r')
        sprintf(tx_buff, "AIN%d: %d\n", channel_index, channels[channel_index]);
      else if (current_command == 'w')
        sprintf(tx_buff, "Valor do DAC: %d\n", dac_value);
      else
        break;
      HAL_UART_Transmit_IT(&huart2, (uint8_t *)tx_buff, strlen(tx_buff));
      HAL_Delay(10);
      break;
    default:
      system_state = SYSTEM_STATE_1;
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
 * @brief I2C1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x10D19CE4;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
   */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
   */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */
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
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
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
  /* DMA1_Channel3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);
  /* DMA1_Channel6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel6_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel6_IRQn);
  /* DMA1_Channel7_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel7_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel7_IRQn);
  /* DMA2_Channel6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Channel6_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Channel6_IRQn);
  /* DMA2_Channel7_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Channel7_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Channel7_IRQn);
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

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

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
