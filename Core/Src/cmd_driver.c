#include "cmd_driver.h"

struct cmd_driver
{
  UART_HandleTypeDef *huart;
  circularBuffer_t *buffer;
  cmd_ready_callback_t callback;
  volatile cmd_state_t state;
};

cmd_driver_t cmd;
volatile char rx_char;
uint16_t cmd_size = 0;

void cmd_driver_init(UART_HandleTypeDef *huart, cmd_ready_callback_t callback)
{
  cmd.huart = huart;
  cmd.buffer = circular_buffer_init(sizeof(char));
  cmd.state = CMD_STATE_IDLE;
  cmd.callback = callback;
}

cmd_state_t cmd_get_state(void)
{
  return cmd.state;
}

void cmd_tick()
{
  switch (cmd.state)
  {
  case CMD_STATE_IDLE:
    HAL_UART_Receive_IT(cmd.huart, &rx_char, 1);
    break;
  case CMD_STATE_RECEIVING:
    __WFI(); // Wait for interrupt - saves power
    break;
  case CMD_STATE_READY:
    if (cmd.callback != NULL)
    {
      char command[cmd_size + 1];
      for (uint16_t i = 0; i < cmd_size; i++)
        circular_buffer_pop(cmd.buffer, &command[i]);
      cmd.callback(command, cmd_size);
      cmd_size = 0;
    }
    break;
  default:
    break;
  }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == cmd.huart->Instance)
  {
    char received_char = rx_char;
    // Check if we received the terminator character
    if ((received_char == '\n' || received_char == '\r' || received_char == ';')) // Your terminator
    {
      circular_buffer_push(cmd.buffer, "\0"); // Null terminate
      cmd.state = CMD_STATE_READY;
    }
    else if (circular_buffer_push(cmd.buffer, &received_char))
    {
      cmd_size++;
      cmd.state = CMD_STATE_RECEIVING;
    }

    // Continue receiving next character
    if (HAL_OK != HAL_UART_Receive_IT(huart, &rx_char, 1))
      cmd.state = CMD_STATE_IDLE;
  }
}