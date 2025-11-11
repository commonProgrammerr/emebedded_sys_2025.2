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
    static char command_buff[MAX_COMMAND_SIZE];
    size_t cmd_size = 0;
    memset(command_buff, '\0', MAX_COMMAND_SIZE);
    for (; command_buff[cmd_size] != '\0' && cmd_size < MAX_COMMAND_SIZE; cmd_size++)
      circular_buffer_pop(cmd.buffer, &command_buff[cmd_size]);

    if (cmd.callback != NULL && cmd_size > 0) 
      cmd.callback(command_buff, cmd_size + 1);

    if (circular_buffer_empty(cmd.buffer))
      cmd.state = CMD_STATE_IDLE;
    break;
  default:
    circular_buffer_destroy(cmd.buffer);
    cmd.buffer = circular_buffer_init(sizeof(char));
    cmd.state = CMD_STATE_IDLE;
    break;
  }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == cmd.huart->Instance)
  {
    char received_char = rx_char;
    // Check if we received the terminator character
    if ((received_char == '\n' || received_char == ';' || received_char == ' '))
    {
      circular_buffer_push(cmd.buffer, "\0"); // Null terminate
      cmd.state = CMD_STATE_READY;
    }
    else if (circular_buffer_push(cmd.buffer, &received_char))
      cmd.state = CMD_STATE_RECEIVING;

    // Continue receiving next character
    if (HAL_OK != HAL_UART_Receive_IT(huart, &rx_char, 1))
      cmd.state = CMD_STATE_IDLE;
  }
}