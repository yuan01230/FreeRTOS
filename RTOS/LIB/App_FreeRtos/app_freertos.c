/*
 * ============================================================================
 *                    FreeRTOS 队列 timeout 实验 C
 * ============================================================================
 *
 * 【当前程序测试内容】
 *
 * 本程序专门观察消息队列在“队列满”和“队列空”时的等待行为：
 *
 *   1. PUT 测试：队列满时，生产者发送消息是否等待、成功或超时；
 *   2. GET 测试：队列空时，消费者接收消息是否等待、成功或超时。
 *
 * 队列长度被设置为 1，便于快速制造边界条件。
 * 程序通过串口输出实验结果，不再使用 LED、TFTLCD 或字库显示。
 *
 * 【如何切换 PUT / GET 测试】
 *
 * 修改下面的 APP_QUEUE_TEST_MODE，然后重新编译并烧录：
 *
 *   #define APP_QUEUE_TEST_MODE APP_QUEUE_TEST_PUT
 *   // 测试队列满时的 osMessageQueuePut()
 *
 *   #define APP_QUEUE_TEST_MODE APP_QUEUE_TEST_GET
 *   // 测试队列空时的 osMessageQueueGet()
 *
 * 【如何切换立即返回 / 等待超时】
 *
 * 修改 APP_QUEUE_WAIT_TICKS：
 *
 *   #define APP_QUEUE_WAIT_TICKS 0U
 *   // 不等待，条件不满足时立即返回 osErrorResource
 *
 *   #define APP_QUEUE_WAIT_TICKS 500U
 *   // 最多等待 500 tick，超时后返回 osErrorTimeout
 *
 *   #define APP_QUEUE_WAIT_TICKS 2000U
 *   // 最多等待 2000 tick；当前默认值，通常可以等到对方任务完成操作
 *
 * 注意：timeout 的单位是 tick，不一定等于毫秒。
 * 具体换算关系取决于 FreeRTOS 的 configTICK_RATE_HZ 配置。
 *
 * 【推荐测试顺序】
 *
 *   ① PUT + timeout=0：观察队列满时立即失败；
 *   ② PUT + timeout=500：观察队列满时等待后超时；
 *   ③ PUT + timeout=2000：观察消费者取走消息后发送成功；
 *   ④ GET + timeout=0：观察队列空时立即失败；
 *   ⑤ GET + timeout=500：观察等待消息后超时；
 *   ⑥ GET + timeout=2000：观察生产者发送消息后接收成功。
 *
 * 每次只修改宏定义，重新编译、烧录，然后观察串口中的：
 *   status=osOK             操作成功；
 *   status=osErrorResource  timeout=0 时队列不可用，立即返回；
 *   status=osErrorTimeout   等待指定 tick 后仍未满足条件；
 *   elapsed                 本次队列 API 实际阻塞了多少 tick。
 *
 * ============================================================================
 */

#include "app_freertos.h"
#include "../UART/app_uart.h"
#include "cmsis_os.h"
#include <stdio.h>

/*
 * 队列实验 C：测试队列满、队列空时的 timeout 行为。
 *
 * 这里故意把队列长度设置为 1：
 *   - 放入第 1 条消息后，队列马上变满；
 *   - 取出消息后，队列马上出现 1 个空位。
 *
 * 队列越短，越容易稳定地观察 osMessageQueuePut() 和
 * osMessageQueueGet() 的阻塞、唤醒和超时过程。
 */
#define APP_QUEUE_LENGTH         1U

/* 实验模式：PUT 测试队列满时的发送，GET 测试队列空时的接收。 */
#define APP_QUEUE_TEST_PUT       1U
#define APP_QUEUE_TEST_GET       2U

/* 修改此宏后重新编译、烧录，即可切换两种实验。 */
#define APP_QUEUE_TEST_MODE      APP_QUEUE_TEST_GET

/*
 * 最长等待时间，单位是 tick，不一定等于毫秒。
 * 例如系统 tick 周期为 1ms 时，2000 tick 才约等于 2000ms。
 *   - timeout = 0：不等待，条件不满足时立即返回；
 *   - timeout > 0：最多等待指定 tick 数；
 *   - osWaitForever：一直等待，直到操作成功。
 */
#define APP_QUEUE_WAIT_TICKS     2000U

/*
 * GET 实验中，生产者延时一段时间后才发送消息；
 * PUT 实验中，消费者延时一段时间后才取消息。
 * 这样可以人为制造“队列空”和“队列满”。
 */
#define APP_QUEUE_RELEASE_TICKS  1000U

typedef struct
{
  /* 用于区分每一轮实验产生的消息。 */
  uint32_t sequence;
} AppQueueMessage_t;

static void App_QueueProducerTask(void *argument);
static void App_QueueConsumerTask(void *argument);
static const char *App_QueueStatusName(osStatus_t status);
static void App_QueuePrintResult(const char *operation, uint32_t sequence,
                                 osStatus_t status, uint32_t start_tick,
                                 uint32_t end_tick);

/* 队列句柄：生产者和消费者通过它访问同一个消息队列。 */
static osMessageQueueId_t appQueueHandle;

/* 生产者优先级为 Normal，负责向队列发送消息。 */
static const osThreadAttr_t queueProducerTaskAttributes = {
  .name = "queueProducer",
  .stack_size = 256 * 4,
  .priority = (osPriority_t)osPriorityNormal,
};

/*
 * 消费者优先级为 Low。
 * PUT 实验中，生产者在队列满时阻塞，消费者仍然可以获得 CPU，
 * 延时后取走消息并释放队列空间。
 */
static const osThreadAttr_t queueConsumerTaskAttributes = {
  .name = "queueConsumer",
  .stack_size = 256 * 4,
  .priority = (osPriority_t)osPriorityLow,
};

void App_FreeRTOS_Init(void)
{
  /*
   * 创建一个“消息数量为 1、消息大小为结构体大小”的队列。
   * 队列会复制消息内容，而不是保存 message 局部变量的地址。
   */
  appQueueHandle = osMessageQueueNew(APP_QUEUE_LENGTH, sizeof(AppQueueMessage_t), NULL);
  if (appQueueHandle == NULL)
  {
    /* 队列创建失败时无法继续进行实验。 */
    App_UART_Print("[timeout] queue create failed\r\n");
    return;
  }

  /* 队列创建成功后，再创建生产者和消费者，避免任务访问无效队列。 */
  osThreadNew(App_QueueProducerTask, NULL, &queueProducerTaskAttributes);
  osThreadNew(App_QueueConsumerTask, NULL, &queueConsumerTaskAttributes);
}

/*
 * 生产者任务：负责调用 osMessageQueuePut() 发送消息。
 *
 * GET 模式：
 *   生产者先延时 1000 tick，给消费者制造“空队列等待”的机会，
 *   然后使用 timeout=0 发送一条消息。
 *
 * PUT 模式：
 *   先放入第 1 条消息使队列变满；
 *   再放入第 2 条消息，并使用 APP_QUEUE_WAIT_TICKS 等待队列空间。
 *   消费者稍后取走第 1 条消息后，第 2 次发送可能被唤醒并成功。
 */
static void App_QueueProducerTask(void *argument)
{
  AppQueueMessage_t message;
  uint32_t sequence = 0U;
  uint32_t start_tick;
  uint32_t end_tick;
  osStatus_t status;

  /* 本实验不使用任务参数。 */
  (void)argument;
  message.sequence = 0U;

#if APP_QUEUE_TEST_MODE == APP_QUEUE_TEST_GET
  /* GET 模式：消费者先等待，生产者 1000 tick 后再提供消息。 */
  App_UART_Print("\r\n[timeout] GET test start\r\n");
  for (;;)
  {
    osDelay(APP_QUEUE_RELEASE_TICKS);
    sequence++;
    message.sequence = sequence;

    /* timeout=0：队列有空间就立即成功，队列满就立即失败。 */
    start_tick = osKernelGetTickCount();
    status = osMessageQueuePut(appQueueHandle, &message, 0U, 0U);
    end_tick = osKernelGetTickCount();
    App_QueuePrintResult("put", sequence, status, start_tick, end_tick);
    osDelay(3000U);
  }
#else
  /* PUT 模式：连续发送两条消息，第二次发送专门测试等待行为。 */
  App_UART_Print("\r\n[timeout] PUT test start\r\n");
  for (;;)
  {
    sequence++;
    message.sequence = sequence;

    /* 清空上一轮可能遗留的消息，保证每轮实验从空队列开始。 */
    osMessageQueueReset(appQueueHandle);

    /* 第 1 条消息应立即进入队列，此时队列 count=1、space=0。 */
    start_tick = osKernelGetTickCount();
    status = osMessageQueuePut(appQueueHandle, &message, 0U, 0U);
    end_tick = osKernelGetTickCount();
    App_QueuePrintResult("put-1", sequence, status, start_tick, end_tick);

    /*
     * 第 2 条消息遇到满队列：
     *   - 消费者在等待时间内取走消息：发送成功，elapsed 约为等待时间；
     *   - 消费者一直没有取消息：等待 APP_QUEUE_WAIT_TICKS 后超时。
     */
    start_tick = osKernelGetTickCount();
    status = osMessageQueuePut(appQueueHandle, &message, 0U, APP_QUEUE_WAIT_TICKS);
    end_tick = osKernelGetTickCount();
    App_QueuePrintResult("put-2", sequence, status, start_tick, end_tick);
    osDelay(3000U);
  }
#endif
}

/*
 * 消费者任务：负责调用 osMessageQueueGet() 接收消息。
 *
 * GET 模式中，队列一开始为空，消费者使用有限 timeout 等待生产者发送。
 * PUT 模式中，消费者先延时，等生产者把队列填满后再取出消息，
 * 从而让正在等待队列空间的生产者有机会继续运行。
 */
static void App_QueueConsumerTask(void *argument)
{
  AppQueueMessage_t message;
  uint32_t sequence = 0U;
  uint32_t start_tick;
  uint32_t end_tick;
  osStatus_t status;

  (void)argument;
  message.sequence = 0U;

#if APP_QUEUE_TEST_MODE == APP_QUEUE_TEST_GET
  /* GET 模式每轮开始前清空队列，确保接收操作确实面对空队列。 */
  App_UART_Print("[timeout] consumer waits for an empty queue\r\n");
  for (;;)
  {
    // osMessageQueueReset(appQueueHandle);
    /* 队列为空时，消费者会阻塞，直到收到消息或等待超时。 */
    start_tick = osKernelGetTickCount();
    status = osMessageQueueGet(appQueueHandle, &message, NULL, APP_QUEUE_WAIT_TICKS);
    end_tick = osKernelGetTickCount();
    App_QueuePrintResult("get", ++sequence, status, start_tick, end_tick);
    osDelay(3000U);
  }
#else
  /* PUT 模式中，延时用于故意保持队列满一段时间。 */
  App_UART_Print("[timeout] consumer releases queue after 1000 ticks\r\n");
  for (;;)
  {
    osDelay(APP_QUEUE_RELEASE_TICKS);
    /* timeout=0：只尝试一次，不等待队列中的消息。 */
    start_tick = osKernelGetTickCount();
    status = osMessageQueueGet(appQueueHandle, &message, NULL, 0U);
    end_tick = osKernelGetTickCount();
    App_QueuePrintResult("get", message.sequence, status, start_tick, end_tick);
    osDelay(3000U);
  }
#endif
}

static const char *App_QueueStatusName(osStatus_t status)
{
  /* 把 CMSIS-RTOS2 的数值返回值转换成容易阅读的字符串。 */
  switch (status)
  {
    case osOK:
      return "osOK";             /* 操作成功。 */
    case osErrorResource:
      return "osErrorResource";  /* timeout=0 时资源不可用，例如队列满或空。 */
    case osErrorTimeout:
      return "osErrorTimeout";   /* 等待了指定时间，但条件仍未满足。 */
    default:
      return "osErrorOther";     /* 其它错误，例如参数或中断上下文错误。 */
  }
}

/*
 * 统一打印一次队列操作的结果。
 *
 * 日志只保留最关键的字段：
 *   op      操作类型，例如 put-1、put-2、get；
 *   seq     消息序号；
 *   status  操作返回值；
 *   elapsed 实际阻塞时间，单位为 tick；
 *   count   当前队列中已有的消息数量；
 *   space   当前队列剩余空间。
 */
static void App_QueuePrintResult(const char *operation, uint32_t sequence,
                                 osStatus_t status, uint32_t start_tick,
                                 uint32_t end_tick)
{
  char text[160];
  uint32_t count = osMessageQueueGetCount(appQueueHandle);
  uint32_t space = osMessageQueueGetSpace(appQueueHandle);

  snprintf(text, sizeof(text),
           "[timeout] op=%s seq=%lu status=%s elapsed=%lu count=%lu space=%lu\r\n",
           operation, (unsigned long)sequence, App_QueueStatusName(status),
           (unsigned long)(end_tick - start_tick),
           (unsigned long)count, (unsigned long)space);
  App_UART_Print(text);
}
