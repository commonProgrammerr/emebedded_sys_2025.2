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
  PCF8591_OK = 1,
  PCF8591_WAITING = 4,
  PCF8591_IDLE = 3
} PCF8591_Status;

typedef enum
{
  SYSTEM_STATE_1 = 0,
  SYSTEM_STATE_2 = 1,
  SYSTEM_STATE_3 = 2,
  SYSTEM_STATE_4 = 3,
  SYSTEM_STATE_5 = 4,
  SYSTEM_STATE_6 = 5
} SystemState;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define PCF8591_ADDRESS (0x48 << 1) // Shifted PCF8591 I2C address
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart2_rx;
DMA_HandleTypeDef hdma_usart2_tx;

/* USER CODE BEGIN PV */
volatile PCF8591_Status pcf8591_status = PCF8591_IDLE;
volatile SystemState system_state = SYSTEM_STATE_1;
volatile uint8_t channels[4];
volatile uint16_t current_command = 0;
volatile uint8_t dac_value = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void next_state(void)
{
  switch (system_state)
  {
  case SYSTEM_STATE_1:
  case SYSTEM_STATE_2:
  case SYSTEM_STATE_3:
  case SYSTEM_STATE_5:
    system_state++;
    break;
  case SYSTEM_STATE_4:
    break;
    system_state = SYSTEM_STATE_1;
    break;
  case SYSTEM_STATE_6:
    system_state = SYSTEM_STATE_4;
    break;
  default:
    system_state = SYSTEM_STATE_1;
    break;
  }
}

void PCF8591_UpdateAnalogChannelData(PCF8591_Channel channel)
{
  uint8_t config_byte = 0x40 | (channel & 0x03); // Select the channel (A0, A1, A2, A3)
  uint8_t analog_data[2];
  // Send configuration byte to select the ADC channel
  HAL_I2C_Master_Transmit_IT(&hi2c1, PCF8591_ADDRESS, &config_byte, 1);
  HAL_Delay(1); // Small delay to allow ADC to settle

  while (pcf8591_status == PCF8591_IDLE)
    ;           // Wait if I2C is busy
  HAL_Delay(1); // Small delay to ensure data is ready

  // Read two bytes: first byte is a dummy, second byte is the actual analog value
  HAL_I2C_Master_Receive_IT(&hi2c1, PCF8591_ADDRESS, analog_data, 2);

  while (pcf8591_status == PCF8591_WAITING) // Wait if I2C is waiting
    HAL_Delay(1);                           // Small delay to ensure data is ready

  // Return the second byte which contains the valid ADC reading
  channels[channel] = analog_data[1];

  pcf8591_status = PCF8591_IDLE; // Reset status to IDLE
}

void PCF8591_SetAnalogOutput(uint8_t dac_output)
{
  uint8_t config_byte = 0b01000000; // Enable analog output
  uint8_t send_data[2] = {
      config_byte,
      dac_output};
  // Send configuration byte to select the ADC channel
  HAL_I2C_Master_Transmit_IT(&hi2c1, PCF8591_ADDRESS, send_data, 2);
  HAL_Delay(1); // Small delay to allow ADC to settle

  while (pcf8591_status == PCF8591_IDLE)
    ;           // Wait if I2C is busy
  HAL_Delay(1); // Small delay to ensure data is ready

  pcf8591_status = PCF8591_IDLE; // Reset status to IDLE
}

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
  if (hi2c->Instance == I2C1)
  {
    if (pcf8591_status == PCF8591_IDLE)
      pcf8591_status = PCF8591_WAITING; // Transmission complete

    if (system_state == SYSTEM_STATE_2)
      system_state = SYSTEM_STATE_3; // Transmission complete
    else if (system_state == SYSTEM_STATE_6)
      system_state = SYSTEM_STATE_4; // Transmission complete
  }
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
  if (hi2c->Instance == I2C1)
  {
    if (pcf8591_status == PCF8591_WAITING)
      pcf8591_status = PCF8591_OK; // Transmission complete

    if (system_state == SYSTEM_STATE_3)
      system_state = SYSTEM_STATE_4; // Transmission complete
  }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2)
  {
    if (system_state == SYSTEM_STATE_4)
    {
      current_command = 0;
      system_state = SYSTEM_STATE_1; // Transmission complete
    }
  }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2)
  {
    if (system_state == SYSTEM_STATE_1)
    {
      if (huart->pRxBuffPtr[0] == 'w')
      {
        system_state = SYSTEM_STATE_5;
        char dac_str[huart->RxXferCount];
        HAL_UART_Receive_IT(&huart2, (uint8_t *)&dac_str, huart->RxXferCount);
      }
      else if (huart->pRxBuffPtr[0] >= '0' && huart->pRxBuffPtr[0] <= '3')
        system_state = SYSTEM_STATE_2; // Reception complete
      else
        HAL_UART_Receive_IT(&huart2, (uint8_t *)&current_command, 1);
    }
    else if (system_state == SYSTEM_STATE_5)
    {
      uint16_t received_size = huart->RxXferSize - huart->RxXferCount;
      char dac_str[received_size];
      memcpy(dac_str, huart->pRxBuffPtr, received_size);
      dac_value = (uint8_t)atoi(dac_str);
      system_state = SYSTEM_STATE_6; // Reception complete
    }
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
  /* USER CODE BEGIN 2 */
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    switch (system_state)
    {
    case SYSTEM_STATE_1:
      HAL_UART_Receive_IT(&huart2, (uint8_t *)&current_command, 1);
    case SYSTEM_STATE_3:
    case SYSTEM_STATE_5:
      HAL_Delay(100); // Small delay to ensure data is ready
      break;
    case SYSTEM_STATE_2:
      PCF8591_UpdateAnalogChannelData((PCF8591_Channel)(current_command - '0'));
      HAL_Delay(10); // Small delay to ensure data is ready
      break;
    case SYSTEM_STATE_6:
      PCF8591_SetAnalogOutput(dac_value);
      HAL_Delay(10); // Small delay to ensure data is ready
    case SYSTEM_STATE_4:
      char tx_buff[100];
      if (current_command != 'w')
        sprintf(tx_buff, "AIN%d: %d\n", current_command - '0', channels[current_command - '0']);
      else
        sprintf(tx_buff, "Valor do DAC: %d\n", dac_value);

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
  hi2c1.Init.Timing = 0x00B10E24;
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

  /** I2C Fast mode Plus enable
   */
  HAL_I2CEx_EnableFastModePlus(I2C_FASTMODEPLUS_I2C1);
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */
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

  /* DMA interrupt init */
  /* DMA1_Channel6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel6_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel6_IRQn);
  /* DMA1_Channel7_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel7_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel7_IRQn);
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
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

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
