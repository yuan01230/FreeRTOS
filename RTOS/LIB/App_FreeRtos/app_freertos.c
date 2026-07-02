#include "app_freertos.h"
#include "../LED/app_led.h"
#include "../UART/app_uart.h"
#include "cmsis_os.h"
#include "font_boot.h"
#include "tftlcd.h"
#include <stdio.h>

static void App_LED0_Task(void *argument);
static void App_LED1_Task(void *argument);
static void App_UART_Task(void *argument);
static void App_LCD_Task(void *argument);
static void App_LCD_FontBootStatus(const char *stage, uint8_t percent, void *user);

static osThreadId_t led0TaskHandle;
static osThreadId_t led1TaskHandle;
static osThreadId_t uartTaskHandle;
static osThreadId_t lcdTaskHandle;

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

static const osThreadAttr_t lcdTaskAttributes = {
  .name = "lcdTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};

/**
 * @brief 创建应用层 FreeRTOS 任务。
 *
 * 当前示例先创建 LCD/字库启动任务，字库准备完成后再创建 LED 和 UART 任务。
 *
 * @param 无。
 * @retval 无。
 */
void App_FreeRTOS_Init(void)
{
  /* 先只创建 LCD/字库启动任务，字库准备完成后再创建其它业务任务。 */
  lcdTaskHandle = osThreadNew(App_LCD_Task, NULL, &lcdTaskAttributes);

  (void)lcdTaskHandle;
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

  App_UART_Print("\r\nFreeRTOS scheduler start.\r\n");

  for (;;)
  {
    /* 每 1 秒通过 USART1 输出一次系统节拍，证明调度器正在运行 */
    snprintf(message, sizeof(message),
             "[scheduler] tick=%lu, uart priority > led priority\r\n",
             (unsigned long)osKernelGetTickCount());
    App_UART_Print(message);
    osDelay(1000);
  }
}

static void App_LCD_FontBootStatus(const char *stage, uint8_t percent, void *user)
{
  char percent_text[16];
  char uart_text[64];
  static const char *last_stage;
  static uint8_t last_percent = 0xFFU;

  (void)user;

  LCD_Fill(16U, 56U, 280U, 96U, WHITE);
  FRONT_COLOR = BLACK;
  LCD_ShowString(16U, 56U, 220U, 20U, 16U, (uint8_t *)stage);
  snprintf(percent_text, sizeof(percent_text), "%u%%", (unsigned int)percent);
  LCD_ShowString(16U, 76U, 80U, 20U, 16U, (uint8_t *)percent_text);

  if ((stage != last_stage) || (percent != last_percent))
  {
    snprintf(uart_text, sizeof(uart_text), "[font] %s %u%%\r\n",
             stage, (unsigned int)percent);
    App_UART_Print(uart_text);
    last_stage = stage;
    last_percent = percent;
  }
}

static void App_LCD_Task(void *argument)
{
  char tick_text[32];
  char font_status[48];
  uint8_t sd_mounted = 0U;
  FontBootResult font_result;

  (void)argument;

  TFTLCD_Init();

  BACK_COLOR = WHITE;
  FRONT_COLOR = BLACK;
  LCD_Clear(WHITE);

  FRONT_COLOR = BLUE;
  LCD_ShowString(16U, 20U, 220U, 24U, 16U, (uint8_t *)"FreeRTOS TFTLCD");

  font_result = FontBoot_EnsureReadyEx(&sd_mounted, App_LCD_FontBootStatus, NULL);
  snprintf(font_status, sizeof(font_status), "font:%s sd:%u",
           FontBoot_ResultToString(font_result), (unsigned int)sd_mounted);
  LCD_Fill(16U, 56U, 280U, 96U, WHITE);
  FRONT_COLOR = BLACK;
  LCD_ShowString(16U, 56U, 250U, 20U, 16U, (uint8_t *)font_status);

  led0TaskHandle = osThreadNew(App_LED0_Task, NULL, &led0TaskAttributes);
  led1TaskHandle = osThreadNew(App_LED1_Task, NULL, &led1TaskAttributes);
  uartTaskHandle = osThreadNew(App_UART_Task, NULL, &uartTaskAttributes);
  (void)led0TaskHandle;
  (void)led1TaskHandle;
  (void)uartTaskHandle;
  (void)osThreadSetPriority(osThreadGetId(), osPriorityLow);

  FRONT_COLOR = RED;
  LCD_ShowTextUtf8(16U, 88U, 260U, 24U, "你好 FreeRTOS");

  FRONT_COLOR = BLACK;
  LCD_ShowTextUtf8(16U, 120U, 280U, 24U, "汉字显示 中文字库");
  LCD_DrawRectangle(10U, 12U, 310U, 150U);

  for (;;)
  {
    snprintf(tick_text, sizeof(tick_text), "tick=%lu",
             (unsigned long)osKernelGetTickCount());
    LCD_Fill(16U, 164U, 160U, 184U, WHITE);
    FRONT_COLOR = BLACK;
    LCD_ShowString(16U, 164U, 150U, 20U, 16U, (uint8_t *)tick_text);
    osDelay(1000);
  }
}
