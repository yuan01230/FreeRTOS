#ifndef __APP_LED_H
#define __APP_LED_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LED 编号。
 *
 * 和 CubeMX 生成的 main.h 中 LED0/LED1 引脚宏对应。
 */
typedef enum
{
  APP_LED0 = 0,
  APP_LED1
} AppLedId;

/**
 * @brief 点亮指定 LED。
 * @param led LED 编号，可选 APP_LED0 或 APP_LED1。
 * @retval 无。
 */
void App_LED_On(AppLedId led);

/**
 * @brief 熄灭指定 LED。
 * @param led LED 编号，可选 APP_LED0 或 APP_LED1。
 * @retval 无。
 */
void App_LED_Off(AppLedId led);

/**
 * @brief 翻转指定 LED 当前状态。
 * @param led LED 编号，可选 APP_LED0 或 APP_LED1。
 * @retval 无。
 */
void App_LED_Toggle(AppLedId led);

#ifdef __cplusplus
}
#endif

#endif /* __APP_LED_H */
