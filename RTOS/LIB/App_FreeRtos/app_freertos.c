#include "app_freertos.h"
#include "../LED/app_led.h"
#include "../UART/app_uart.h"
#include "cmsis_os.h"
#include "font_boot.h"
#include "tftlcd.h"
#include <stdio.h>

/* LCD 显示消息队列长度：最多缓存 8 条待显示消息。 */
#define APP_LCD_QUEUE_LENGTH 8U
/* 每条 LCD 显示消息携带的文本最大长度，直接作为结构体成员随队列复制。 */
#define APP_LCD_TEXT_MAX     48U
/* 队列实验模式：当前选择实验 A，用快速生产、慢速消费观察队列满。 */
#define APP_QUEUE_EXPERIMENT_NORMAL      0U
#define APP_QUEUE_EXPERIMENT_QUEUE_FULL  1U
#define APP_QUEUE_EXPERIMENT             APP_QUEUE_EXPERIMENT_QUEUE_FULL

#if APP_QUEUE_EXPERIMENT == APP_QUEUE_EXPERIMENT_QUEUE_FULL
/* 实验 A：生产者每 10ms 发一条消息，消费者每处理一条后延时 500ms。 */
#define APP_LCD_PRODUCER_PERIOD_MS       10U
#define APP_LCD_CONSUMER_DELAY_MS        500U
#else
/* 正常模式：生产和消费速度接近，用于观察无积压队列。 */
#define APP_LCD_PRODUCER_PERIOD_MS       1000U
#define APP_LCD_CONSUMER_DELAY_MS        0U
#endif

/* LCD 显示消息类型：后续可根据类型扩展不同显示区域或处理方式。 */
typedef enum
{
  APP_LCD_MSG_STATUS = 0,
  APP_LCD_MSG_TICK,
  APP_LCD_MSG_LED,
  APP_LCD_MSG_ERROR,
} AppLcdMessageType_t;

/* 队列中传递的 LCD 显示消息。
 * 注意：text 是结构体内部数组，队列发送时复制的是完整消息副本，
 * 不依赖发送任务中的局部字符串地址。
 */
typedef struct
{
  AppLcdMessageType_t type;
  uint32_t sequence;
  uint32_t tick;
  uint16_t x;
  uint16_t y;
  uint16_t color;
  char text[APP_LCD_TEXT_MAX];
} AppLcdMessage_t;

static void App_LED0_Task(void *argument);
static void App_LED1_Task(void *argument);
static void App_UART_Task(void *argument);
static void App_LCD_Task(void *argument);
static void App_LCD_Producer_Task(void *argument);
static void App_LCD_FontBootStatus(const char *stage, uint8_t percent, void *user);
static void App_LCD_DrawMessage(const AppLcdMessage_t *message);

static osThreadId_t led0TaskHandle;
static osThreadId_t led1TaskHandle;
static osThreadId_t uartTaskHandle;
static osThreadId_t lcdTaskHandle;
static osThreadId_t lcdProducerTaskHandle;
/* LCD 显示消息队列句柄，生产者任务写入消息，lcdTask 读取并刷新屏幕。 */
static osMessageQueueId_t lcdQueueHandle;
/* 记录生产者发送失败次数，用于观察队列满或发送异常的情况。 */
static volatile uint32_t lcdProducerDropCount;

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

static const osThreadAttr_t lcdProducerTaskAttributes = {
  .name = "lcdProducerTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
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
 *
 * 该任务用于辅助观察队列实验状态，每 1 秒打印一次：
 * - 当前系统 tick；
 * - LCD 队列中已有消息数量；
 * - LCD 队列剩余空间；
 * - LCD 生产者任务发送失败次数。
 *
 * @param argument 任务参数，本示例未使用。
 * @retval 无。FreeRTOS 任务函数不应返回。
 */
static void App_UART_Task(void *argument)
{
  char message[96];
  uint32_t queue_count;
  uint32_t queue_space;

  (void)argument;

  App_UART_Print("\r\nFreeRTOS queue TFTLCD demo start.\r\n");

  for (;;)
  {
    /* 周期性观察队列状态：当前消息数、剩余空间、发送失败次数。 */
    queue_count = (lcdQueueHandle != NULL) ? osMessageQueueGetCount(lcdQueueHandle) : 0U;
    queue_space = (lcdQueueHandle != NULL) ? osMessageQueueGetSpace(lcdQueueHandle) : 0U;
    snprintf(message, sizeof(message),
             "[queue] tick=%lu count=%lu space=%lu drops=%lu\r\n",
             (unsigned long)osKernelGetTickCount(),
             (unsigned long)queue_count,
             (unsigned long)queue_space,
             (unsigned long)lcdProducerDropCount);
    App_UART_Print(message);
    osDelay(1000);
  }
}

/**
 * @brief LCD 消息生产者任务。
 *
 * 该任务不直接操作 TFTLCD，只负责周期性生成 AppLcdMessage_t，
 * 然后通过 lcdQueueHandle 发送给 lcdTask。这样可以避免多个任务同时刷屏。
 *
 * @param argument 任务参数，本示例未使用。
 * @retval 无。FreeRTOS 任务函数不应返回。
 */
static void App_LCD_Producer_Task(void *argument)
{
  AppLcdMessage_t message;
  uint32_t sequence = 0U;
  osStatus_t status;

  (void)argument;

  for (;;)
  {
    /* 构造一条 tick 显示消息，消息内容会被 osMessageQueuePut() 复制进队列。 */
    sequence++;
    message.type = APP_LCD_MSG_TICK;
    message.sequence = sequence;
    message.tick = osKernelGetTickCount();
    message.x = 16U;
    message.y = 164U;
    message.color = BLACK;
    snprintf(message.text, sizeof(message.text), "seq=%lu tick=%lu",
             (unsigned long)message.sequence,
             (unsigned long)message.tick);

    /* timeout = 0 表示队列满时不等待，立即返回失败，便于观察丢消息计数。 */
    status = osMessageQueuePut(lcdQueueHandle, &message, 0U, 0U);
    if (status != osOK)
    {
      lcdProducerDropCount++;
    }

    osDelay(APP_LCD_PRODUCER_PERIOD_MS);
  }
}

/**
 * @brief 字库启动进度回调函数。
 *
 * FontBoot_EnsureReadyEx() 在挂载 SD 卡、检查字库、加载字库等阶段会调用该函数。
 * 本函数会把当前阶段和百分比显示到 TFTLCD，同时通过串口输出变化后的进度。
 *
 * @param stage 当前启动阶段文本，例如挂载、检查、加载等。
 * @param percent 当前阶段百分比，范围通常为 0 到 100。
 * @param user 用户自定义参数，本示例未使用。
 * @retval 无。
 */
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

/**
 * @brief LCD 主任务入口函数。
 *
 * 该任务负责：
 * 1. 初始化 TFTLCD；
 * 2. 启动并检查中文字库；
 * 3. 创建 LCD 显示消息队列；
 * 4. 创建 LED、UART、LCD 消息生产者任务；
 * 5. 阻塞等待 lcdQueueHandle 中的显示消息；
 * 6. 收到消息后统一调用 App_LCD_DrawMessage() 刷新 TFTLCD。
 *
 * 设计重点：只有 lcdTask 直接操作 TFTLCD，其它任务只能通过队列发送显示请求。
 *
 * @param argument 任务参数，本示例未使用。
 * @retval 无。FreeRTOS 任务函数不应返回。
 */
static void App_LCD_Task(void *argument)
{
  AppLcdMessage_t message;
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

  /* 字库和 LCD 初始化完成后再创建队列，后续所有 LCD 刷新请求都经由该队列进入 lcdTask。 */
  lcdQueueHandle = osMessageQueueNew(APP_LCD_QUEUE_LENGTH, sizeof(AppLcdMessage_t), NULL);
  if (lcdQueueHandle == NULL)
  {
    FRONT_COLOR = RED;
    LCD_ShowString(16U, 88U, 260U, 24U, 16U, (uint8_t *)"lcd queue create failed");
    for (;;)
    {
      osDelay(1000);
    }
  }

  /* 队列创建成功后再启动业务任务，保证生产者发送消息时队列已经可用。 */
  led0TaskHandle = osThreadNew(App_LED0_Task, NULL, &led0TaskAttributes);
  led1TaskHandle = osThreadNew(App_LED1_Task, NULL, &led1TaskAttributes);
  uartTaskHandle = osThreadNew(App_UART_Task, NULL, &uartTaskAttributes);
  lcdProducerTaskHandle = osThreadNew(App_LCD_Producer_Task, NULL, &lcdProducerTaskAttributes);
  (void)led0TaskHandle;
  (void)led1TaskHandle;
  (void)uartTaskHandle;
  (void)lcdProducerTaskHandle;
  (void)osThreadSetPriority(osThreadGetId(), osPriorityLow);

  FRONT_COLOR = RED;
  LCD_ShowTextUtf8(16U, 88U, 260U, 24U, "你好 FreeRTOS");

  FRONT_COLOR = BLACK;
  LCD_ShowTextUtf8(16U, 120U, 280U, 24U, "汉字显示 中文字库");
  LCD_DrawRectangle(10U, 12U, 310U, 150U);
  LCD_DrawRectangle(10U, 156U, 310U, 190U);

  for (;;)
  {
    /* 队列为空时 lcdTask 阻塞在这里，不占用 CPU；收到消息后再刷新 TFTLCD。 */
    if (osMessageQueueGet(lcdQueueHandle, &message, NULL, osWaitForever) == osOK)
    {
      App_LCD_DrawMessage(&message);
#if APP_LCD_CONSUMER_DELAY_MS > 0U
      /* 实验 A：故意降低 lcdTask 消费速度，观察队列逐渐填满和 drops 增加。 */
      osDelay(APP_LCD_CONSUMER_DELAY_MS);
#endif
    }
  }
}

/**
 * @brief 根据 LCD 显示消息刷新屏幕。
 *
 * 只有 lcdTask 调用该函数，避免多个任务同时操作 TFTLCD 造成显示交叉或总线冲突。
 *
 * @param message 从 lcdQueueHandle 收到的显示消息副本。
 * @retval 无。
 */
static void App_LCD_DrawMessage(const AppLcdMessage_t *message)
{
  uint16_t y_end;

  if (message == NULL)
  {
    return;
  }

  y_end = (uint16_t)(message->y + 20U);
  /* 先清除本行区域，再显示最新文本，避免旧字符残留。 */
  LCD_Fill(message->x, message->y, 300U, y_end, WHITE);
  FRONT_COLOR = message->color;
  LCD_ShowString(message->x, message->y, 280U, 20U, 16U, (uint8_t *)message->text);
}
