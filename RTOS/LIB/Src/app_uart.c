#include "app_uart.h"
#include "main.h"
#include <string.h>

extern UART_HandleTypeDef huart1;

/**
 * @brief 通过 USART1 发送字符串。
 * @param message 需要发送的字符串，以 '\0' 结尾；传入 NULL 时函数直接返回。
 * @retval 无。
 */
void App_UART_Print(const char *message)
{
  if (message == NULL)
  {
    return;
  }

  /* 串口示例使用阻塞发送，便于学习；复杂工程可改为队列 + DMA/中断发送。 */
  HAL_UART_Transmit(&huart1, (uint8_t *)message, (uint16_t)strlen(message), HAL_MAX_DELAY);
}
