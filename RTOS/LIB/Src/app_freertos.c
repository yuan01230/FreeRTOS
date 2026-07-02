#include "app_freertos.h"
#include "app_led.h"
#include "app_uart.h"
#include "cmsis_os.h"
#include <stdio.h>

/*
 * Day 2 scheduler experiments:
 * 1 - Preemption: UART task has higher priority than LED tasks, all tasks delay.
 * 2 - Starvation: UART task runs without delay and starves lower-priority LED tasks.
 * 3 - Time slicing: LED tasks have the same priority and stay Ready; UART reports counters.
 */
#define APP_SCHEDULER_EXPERIMENT 1

static void App_LED0_Task(void *argument);
static void App_LED1_Task(void *argument);
static void App_UART_Task(void *argument);

static osThreadId_t led0TaskHandle;
static osThreadId_t led1TaskHandle;
static osThreadId_t uartTaskHandle;

#if APP_SCHEDULER_EXPERIMENT == 3
static volatile uint32_t led0LoopCount;
static volatile uint32_t led1LoopCount;
#endif

static const osThreadAttr_t led0TaskAttributes = {
  .name = "led0Task",
  .stack_size = 128 * 4,
#if APP_SCHEDULER_EXPERIMENT == 3
  .priority = (osPriority_t) osPriorityNormal,
#else
  .priority = (osPriority_t) osPriorityLow,
#endif
};

static const osThreadAttr_t led1TaskAttributes = {
  .name = "led1Task",
  .stack_size = 128 * 4,
#if APP_SCHEDULER_EXPERIMENT == 3
  .priority = (osPriority_t) osPriorityNormal,
#else
  .priority = (osPriority_t) osPriorityLow,
#endif
};

static const osThreadAttr_t uartTaskAttributes = {
  .name = "uartTask",
  .stack_size = 256 * 4,
#if APP_SCHEDULER_EXPERIMENT == 3
  .priority = (osPriority_t) osPriorityAboveNormal,
#else
  .priority = (osPriority_t) osPriorityNormal,
#endif
};

/**
 * @brief 创建应用层 FreeRTOS 任务。
 *
 * 当前示例创建 3 个任务，用 APP_SCHEDULER_EXPERIMENT 选择调度实验。
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
#if APP_SCHEDULER_EXPERIMENT == 3
    led0LoopCount++;
    if ((led0LoopCount % 200000UL) == 0UL)
    {
      App_LED_Toggle(APP_LED0);
    }
#else
    /* LED0 每 500ms 翻转一次，用来观察短周期任务 */
    App_LED_Toggle(APP_LED0);
    osDelay(500);
#endif
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
#if APP_SCHEDULER_EXPERIMENT == 3
    led1LoopCount++;
    if ((led1LoopCount % 200000UL) == 0UL)
    {
      App_LED_Toggle(APP_LED1);
    }
#else
    /* LED1 每 1000ms 翻转一次，用来观察不同任务周期 */
    App_LED_Toggle(APP_LED1);
    osDelay(1000);
#endif
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

  snprintf(message, sizeof(message),
           "\r\nFreeRTOS scheduler experiment %d start.\r\n",
           APP_SCHEDULER_EXPERIMENT);
  App_UART_Print(message);

  for (;;)
  {
#if APP_SCHEDULER_EXPERIMENT == 1
    /* 每 1 秒通过 USART1 输出一次系统节拍，证明调度器正在运行 */
    snprintf(message, sizeof(message),
             "[exp1 preemption] tick=%lu, uart priority > led priority\r\n",
             (unsigned long)osKernelGetTickCount());
    App_UART_Print(message);
    osDelay(1000);
#elif APP_SCHEDULER_EXPERIMENT == 2
    /* This task has higher priority and never blocks, so lower-priority LEDs starve. */
    App_LED_Toggle(APP_LED1);
#elif APP_SCHEDULER_EXPERIMENT == 3
    uint32_t led0Snapshot = led0LoopCount;
    uint32_t led1Snapshot = led1LoopCount;

    snprintf(message, sizeof(message),
             "[exp3 time-slice] tick=%lu, led0=%lu, led1=%lu\r\n",
             (unsigned long)osKernelGetTickCount(),
             (unsigned long)led0Snapshot,
             (unsigned long)led1Snapshot);
    App_UART_Print(message);
    osDelay(1000);
#else
#error "Unsupported APP_SCHEDULER_EXPERIMENT value"
#endif
  }
}
