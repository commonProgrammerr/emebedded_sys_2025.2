#if !defined(CMD_DRIVER_H)
#define CMD_DRIVER_H

#include "main.h"
#include "circular_buffer.h"
#include "string.h"

#define CMD_BUFFER_SIZE 1024
#define MAX_COMMAND_SIZE CMD_BUFFER_SIZE
typedef enum
{
  CMD_STATE_IDLE = 0,
  CMD_STATE_RECEIVING,
  CMD_STATE_READY,
} cmd_state_t;

typedef struct cmd_driver cmd_driver_t;
typedef void (*cmd_ready_callback_t)(char *cmd, uint16_t size);

void cmd_tick(void);
void cmd_driver_init(UART_HandleTypeDef *huart, cmd_ready_callback_t callback);
cmd_state_t cmd_get_state();
#endif // CMD_DRIVER_H
