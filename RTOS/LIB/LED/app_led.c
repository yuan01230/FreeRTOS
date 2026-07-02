#include "app_led.h"
#include "main.h"

/* 普中麒麟 F407 开发板 LED 为低电平点亮，高电平熄灭。 */
#define APP_LED_ON_LEVEL   GPIO_PIN_RESET
#define APP_LED_OFF_LEVEL  GPIO_PIN_SET

/**
 * @brief 获取指定 LED 对应的 GPIO 端口。
 * @param led LED 编号。
 * @retval GPIO 端口指针。
 */
static GPIO_TypeDef *App_LED_GetPort(AppLedId led)
{
  return (led == APP_LED0) ? LED0_GPIO_Port : LED1_GPIO_Port;
}

/**
 * @brief 获取指定 LED 对应的 GPIO 引脚。
 * @param led LED 编号。
 * @retval GPIO 引脚编号。
 */
static uint16_t App_LED_GetPin(AppLedId led)
{
  return (led == APP_LED0) ? LED0_Pin : LED1_Pin;
}

/**
 * @brief 点亮指定 LED。
 * @param led LED 编号。
 * @retval 无。
 */
void App_LED_On(AppLedId led)
{
  HAL_GPIO_WritePin(App_LED_GetPort(led), App_LED_GetPin(led), APP_LED_ON_LEVEL);
}

/**
 * @brief 熄灭指定 LED。
 * @param led LED 编号。
 * @retval 无。
 */
void App_LED_Off(AppLedId led)
{
  HAL_GPIO_WritePin(App_LED_GetPort(led), App_LED_GetPin(led), APP_LED_OFF_LEVEL);
}

/**
 * @brief 翻转指定 LED 当前状态。
 * @param led LED 编号。
 * @retval 无。
 */
void App_LED_Toggle(AppLedId led)
{
  HAL_GPIO_TogglePin(App_LED_GetPort(led), App_LED_GetPin(led));
}
