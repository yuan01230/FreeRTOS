#include "app_freertos.h"
#include "app_led.h"
#include "app_uart.h"
#include "cmsis_os.h"
#include <stdio.h>

static void App_LED0_Task(void *argument);
static void App_LED1_Task(void *argument);
static void App_UART_Task(void *argument);

static osThreadId_t led0TaskHandle;
static osThreadId_t led1TaskHandle;
static osThreadId_t uartTaskHandle;

static const osThreadAttr_t led0TaskAttributes = {
  .name = "led0Task",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow,
};

static const osThreadAttr_t led1TaskAttributes = {
  .name = "led1Task",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow,
};

static const osThreadAttr_t uartTaskAttributes = {
  .name = "uartTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/**
 * @brief 创建应用层 FreeRTOS 任务。
 *
 * 当前示例创建 3 个任务：
 * - LED0 任务：每 500ms 翻转 LED0。
 * - LED1 任务：每 1000ms 翻转 LED1。
 * - 串口任务：每 1000ms 输出一次系统 tick 信息。
 *
 * @param 无。
 * @retval 无。
 */
void App_FreeRTOS_Init(void)
{
  /* 在 osKernelInitialize() 之后、osKernelStart() 之前创建应用任务 */
  led0TaskHandle = osThreadNew(App_LED0_Task, NULL, &led0TaskAttributes);
  led1TaskHandle = osThreadNew(App_LED1_Task, NULL, &led1TaskAttributes);
  uartTaskHandle = osThreadNew(App_UART_Task, NULL, &uartTaskAttributes);

  (void)led0TaskHandle;
  (void)led1TaskHandle;
  (void)uartTaskHandle;
}

/**
 * @brief LED0 闪烁任务入口函数。
 * @param argument 任务参数，本示例未使用。
 * @retval 无。FreeRTOS 任务函数不应返回。
 */
static void App_LED0_Task(void *argument)
{
  (void)argument;

  for (;;)
  {
    /* LED0 每 500ms 翻转一次，用来观察短周期任务 */
    App_LED_Toggle(APP_LED0);
    osDelay(500);
  }
}

/**
 * @brief LED1 闪烁任务入口函数。
 * @param argument 任务参数，本示例未使用。
 * @retval 无。FreeRTOS 任务函数不应返回。
 */
static void App_LED1_Task(void *argument)
{
  (void)argument;

  for (;;)
  {
    /* LED1 每 1000ms 翻转一次，用来观察不同任务周期 */
    App_LED_Toggle(APP_LED1);
    osDelay(1000);
  }
}

/**
 * @brief 串口打印任务入口函数。
 * @param argument 任务参数，本示例未使用。
 * @retval 无。FreeRTOS 任务函数不应返回。
 */
static void App_UART_Task(void *argument)
{
  char message[96];

  (void)argument;

  App_UART_Print("\r\nFreeRTOS demo start: LED0/LED1/UART tasks are running.\r\n");

  for (;;)
  {
    /* 每 1 秒通过 USART1 输出一次系统节拍，证明调度器正在运行 */
    snprintf(message, sizeof(message), "tick=%lu ms, LED0=500ms, LED1=1000ms\r\n",
             (unsigned long)osKernelGetTickCount());
    App_UART_Print(message);
    osDelay(1000);
  }
}
