#include "cmd_driver.h"

cmd_driver_t cmd;
volatile char rx_char;

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
    __WFI(); // Wait for interrupt
    break;
  case CMD_STATE_READY:
    if (cmd.callback != NULL)
    {
      char *command_buff = malloc(MAX_COMMAND_SIZE + 1);
      size_t i;
      for (i = 0; i < MAX_COMMAND_SIZE; i++)
      {
        circular_buffer_pop(cmd.buffer, &command_buff[i]);
        if (command_buff[i] == '\0')
          break;
      }
      if (i > 1)
        cmd.callback(command_buff, strlen(command_buff));
      free(command_buff);
    }

    if (circular_buffer_empty(cmd.buffer))
      cmd.state = CMD_STATE_IDLE;
    break;
  default:
    cmd.state = CMD_STATE_IDLE;
    break;
  }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == cmd.huart->Instance)
  {
    // Check if we received the terminator character
    if ((rx_char == '\n' || rx_char == '\r'))
    {
      circular_buffer_push(cmd.buffer, "\0"); // Null terminate
      cmd.state = CMD_STATE_READY;
    }
    else
    {
      if (cmd.state != CMD_STATE_RECEIVING)
        cmd.state = CMD_STATE_RECEIVING;

      circular_buffer_push(cmd.buffer, &rx_char);
      // Continue receiving next character
      HAL_UART_Receive_IT(huart, &rx_char, 1);
    }
  }
}

size_t cmd_buffer_size()
{
  size_t size = 0;
  size = circular_buffer_capacity(cmd.buffer) - circular_buffer_free_space(cmd.buffer);
  return size;
}
