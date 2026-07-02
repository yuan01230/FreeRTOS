#ifndef __APP_UART_H
#define __APP_UART_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 通过 USART1 发送字符串。
 * @param message 需要发送的字符串，以 '\0' 结尾；传入 NULL 时函数直接返回。
 * @retval 无。
 */
void App_UART_Print(const char *message);

#ifdef __cplusplus
}
#endif

#endif /* __APP_UART_H */
