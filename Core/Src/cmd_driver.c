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
      /* TODO: Check malloc return value for NULL before using command_buff */
      if (command_buff == NULL)
      {
        cmd.state = CMD_STATE_IDLE;
        break;
      }
      
      size_t i;
      /* TODO: Add protection against malformed commands without null terminator
       * that could overflow the buffer or cause indefinite loops. */
      for (i = 0; i < MAX_COMMAND_SIZE; i++)
      {
        /* TODO: Check return value of circular_buffer_pop. If pop fails we
         * should break and avoid using uninitialized data. Also ensure we
         * always null-terminate `command_buff` before calling strlen/callback. */
        if (!circular_buffer_pop(cmd.buffer, &command_buff[i]))
          break;
        if (command_buff[i] == '\0')
          break;
      }
      /* Ensure null-termination in case we hit MAX_COMMAND_SIZE without '\0' */
      if (i == MAX_COMMAND_SIZE)
        command_buff[MAX_COMMAND_SIZE] = '\0';

      if (i > 0)
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
      /* FIXME: Pushing a string literal's address into the circular buffer
       * may be confusing; explicitly push a char variable to avoid passing
       * a pointer to static literal memory (and make intent clear). */
      char zero = '\0';
      circular_buffer_push(cmd.buffer, &zero); // Null terminate
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
