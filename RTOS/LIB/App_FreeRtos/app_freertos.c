/*
 * ============================================================================
 *              FreeRTOS 队列实验 E：静态消息池 + 双队列
 * ============================================================================
 *
 * 【当前程序测试内容】
 *
 * 实验 D 已经证明：队列可以保存指针，消费者能通过指针访问消息对象。
 *
 * 实验 E 继续解决一个更真实的问题：
 *
 *   如果只有一个 staticMessage，生产者下一轮可能覆盖消费者尚未处理完的数据。
 *
 * 当前实验使用 3 个静态消息块组成“消息池”，再使用两个队列管理消息块所有权：
 *
 *   1. freeQueue：保存空闲消息块指针；
 *   2. dataQueue：保存已经填好、等待消费者处理的消息块指针。
 *
 * 【消息块所有权流转】
 *
 *   freeQueue
 *      ↓ 生产者取出空闲块
 *   生产者填写消息
 *      ↓ 生产者放入 dataQueue
 *   dataQueue
 *      ↓ 消费者取出待处理块
 *   消费者处理消息
 *      ↓ 消费者归还空闲块
 *   freeQueue
 *
 * 【如何切换不同测试】
 *
 * 当前文件固定运行实验 E。
 *
 * 可以修改下面几个宏观察不同现象：
 *
 *   APP_MESSAGE_POOL_SIZE
 *     静态消息块数量。当前为 3。
 *
 *   APP_PRODUCER_PERIOD_TICKS
 *     生产者产生消息的间隔。值越小，生产越快。
 *
 *   APP_CONSUMER_WORK_TICKS
 *     消费者模拟处理消息的耗时。值越大，消费越慢。
 *
 *   APP_POOL_WAIT_TICKS
 *     生产者等待空闲消息块的最长时间。
 *     如果消费者太慢，freeQueue 为空，生产者会等待或超时。
 *
 * 【本实验要验证什么】
 *
 *   1. 队列中传递的是消息块指针；
 *   2. 每个消息块同一时间只属于一个位置：freeQueue、生产者、dataQueue 或消费者；
 *   3. 消费者处理完成后必须把消息块归还 freeQueue；
 *   4. 当没有空闲消息块时，生产者不会覆盖正在使用的数据。
 *
 * ============================================================================
 */

#include "app_freertos.h"
#include "../UART/app_uart.h"
#include "cmsis_os.h"
#include <stdint.h>
#include <stdio.h>

/* 静态消息池中的消息块数量，也是两个队列的最大长度。 */
#define APP_MESSAGE_POOL_SIZE       3U

/* 生产者每隔 500 tick 尝试产生一条新消息。 */
#define APP_PRODUCER_PERIOD_TICKS   500U

/* 消费者处理一条消息需要 1500 tick，用来故意制造“消费较慢”的场景。 */
#define APP_CONSUMER_WORK_TICKS     1500U

/*
 * 生产者从 freeQueue 申请空闲块时最多等待 1000 tick。
 * 如果消息池全部被占用，等待超时后本轮消息会被跳过。
 */
#define APP_POOL_WAIT_TICKS         1000U

typedef struct
{
  /*
   * id 是消息块编号，固定不变。
   * 通过 id 可以看出哪个静态块正在被复用。
   */
  uint32_t id;

  /* sequence 是消息序号，每产生一条新消息就递增。 */
  uint32_t sequence;

  /* created_tick 记录生产者填写消息时的系统 tick。 */
  uint32_t created_tick;
} AppQueueMessage_t;

static void App_QueueProducerTask(void *argument);
static void App_QueueConsumerTask(void *argument);
static void App_QueueInitMessagePool(void);
static const char *App_QueueStatusName(osStatus_t status);
static void App_QueuePrintState(const char *operation, const AppQueueMessage_t *message,
                                osStatus_t status);

/*
 * freeQueueHandle 保存空闲消息块指针。
 * 生产者要发送消息前，必须先从这里取一个块。
 */
static osMessageQueueId_t freeQueueHandle;

/*
 * dataQueueHandle 保存待处理消息块指针。
 * 生产者填好消息后放入这里，消费者从这里取走处理。
 */
static osMessageQueueId_t dataQueueHandle;

/*
 * 静态消息池。
 *
 * 所有消息对象都在这里静态分配，不使用 malloc。
 * 每个对象的地址会在 freeQueue 和 dataQueue 之间流转。
 */
static AppQueueMessage_t messagePool[APP_MESSAGE_POOL_SIZE];

static const osThreadAttr_t queueProducerTaskAttributes = {
  .name = "queueProducer",
  .stack_size = 256 * 4,
  .priority = (osPriority_t)osPriorityNormal,
};

static const osThreadAttr_t queueConsumerTaskAttributes = {
  .name = "queueConsumer",
  .stack_size = 256 * 4,
  .priority = (osPriority_t)osPriorityLow,
};

void App_FreeRTOS_Init(void)
{
  /*
   * 两个队列保存的元素都是 AppQueueMessage_t *。
   * 队列复制的是指针值，不复制消息块内容。
   */
  freeQueueHandle = osMessageQueueNew(APP_MESSAGE_POOL_SIZE,
                                      sizeof(AppQueueMessage_t *),
                                      NULL);
  dataQueueHandle = osMessageQueueNew(APP_MESSAGE_POOL_SIZE,
                                      sizeof(AppQueueMessage_t *),
                                      NULL);

  if ((freeQueueHandle == NULL) || (dataQueueHandle == NULL))
  {
    App_UART_Print("[pool] queue create failed\r\n");
    return;
  }

  App_QueueInitMessagePool();

  App_UART_Print("\r\n[pool] experiment E start: static message pool + double queues\r\n");
  App_UART_Print("[pool] flow: freeQueue -> producer -> dataQueue -> consumer -> freeQueue\r\n");
  App_QueuePrintState("init", NULL, osOK);

  osThreadNew(App_QueueProducerTask, NULL, &queueProducerTaskAttributes);
  osThreadNew(App_QueueConsumerTask, NULL, &queueConsumerTaskAttributes);
}

/*
 * 初始化消息池。
 *
 * 每个静态消息块只初始化一次，然后把它的地址放入 freeQueue。
 * 程序运行后，生产者只能从 freeQueue 中取得空闲块。
 */
static void App_QueueInitMessagePool(void)
{
  AppQueueMessage_t *message_ptr;

  for (uint32_t i = 0U; i < APP_MESSAGE_POOL_SIZE; i++)
  {
    messagePool[i].id = i;
    messagePool[i].sequence = 0U;
    messagePool[i].created_tick = 0U;

    message_ptr = &messagePool[i];
    (void)osMessageQueuePut(freeQueueHandle, &message_ptr, 0U, 0U);
  }
}

/*
 * 生产者任务。
 *
 * 每轮流程：
 *   1. 从 freeQueue 申请一个空闲消息块；
 *   2. 填写 sequence 和 created_tick；
 *   3. 把消息块指针放入 dataQueue；
 *   4. 如果没有空闲块，等待 APP_POOL_WAIT_TICKS 后跳过本轮。
 */
static void App_QueueProducerTask(void *argument)
{
  AppQueueMessage_t *message_ptr = NULL;
  uint32_t sequence = 0U;
  osStatus_t status;

  (void)argument;

  for (;;)
  {
    osDelay(APP_PRODUCER_PERIOD_TICKS);

    message_ptr = NULL;
    status = osMessageQueueGet(freeQueueHandle, &message_ptr, NULL, APP_POOL_WAIT_TICKS);
    App_QueuePrintState("alloc", message_ptr, status);

    if ((status == osOK) && (message_ptr != NULL))
    {
      sequence++;
      message_ptr->sequence = sequence;
      message_ptr->created_tick = osKernelGetTickCount();

      status = osMessageQueuePut(dataQueueHandle, &message_ptr, 0U, 0U);
      App_QueuePrintState("put", message_ptr, status);

      /*
       * dataQueue 理论上有足够空间。
       * 如果这里失败，不能丢掉消息块，必须归还 freeQueue。
       */
      if (status != osOK)
      {
        (void)osMessageQueuePut(freeQueueHandle, &message_ptr, 0U, 0U);
        App_QueuePrintState("return", message_ptr, status);
      }
    }
  }
}

/*
 * 消费者任务。
 *
 * 每轮流程：
 *   1. 从 dataQueue 等待一条待处理消息；
 *   2. 模拟处理 APP_CONSUMER_WORK_TICKS；
 *   3. 处理完成后，把消息块归还 freeQueue。
 */
static void App_QueueConsumerTask(void *argument)
{
  AppQueueMessage_t *message_ptr = NULL;
  osStatus_t status;

  (void)argument;

  for (;;)
  {
    message_ptr = NULL;
    status = osMessageQueueGet(dataQueueHandle, &message_ptr, NULL, osWaitForever);
    App_QueuePrintState("get", message_ptr, status);

    if ((status == osOK) && (message_ptr != NULL))
    {
      osDelay(APP_CONSUMER_WORK_TICKS);

      status = osMessageQueuePut(freeQueueHandle, &message_ptr, 0U, 0U);
      App_QueuePrintState("free", message_ptr, status);
    }
  }
}

static const char *App_QueueStatusName(osStatus_t status)
{
  switch (status)
  {
    case osOK:
      return "osOK";
    case osErrorResource:
      return "osErrorResource";
    case osErrorTimeout:
      return "osErrorTimeout";
    default:
      return "osErrorOther";
  }
}

/*
 * 统一打印消息池状态。
 *
 * 日志字段说明：
 *   op      当前动作：init、alloc、put、get、free；
 *   id      消息块编号；
 *   seq     消息序号；
 *   status  队列 API 返回值；
 *   free    freeQueue 中空闲块数量；
 *   data    dataQueue 中待处理块数量。
 */
static void App_QueuePrintState(const char *operation, const AppQueueMessage_t *message,
                                osStatus_t status)
{
  char text[192];
  uint32_t free_count = osMessageQueueGetCount(freeQueueHandle);
  uint32_t data_count = osMessageQueueGetCount(dataQueueHandle);

  if (message != NULL)
  {
    snprintf(text, sizeof(text),
             "[pool] op=%s id=%lu seq=%lu status=%s free=%lu data=%lu\r\n",
             operation,
             (unsigned long)message->id,
             (unsigned long)message->sequence,
             App_QueueStatusName(status),
             (unsigned long)free_count,
             (unsigned long)data_count);
  }
  else
  {
    snprintf(text, sizeof(text),
             "[pool] op=%s id=- seq=- status=%s free=%lu data=%lu\r\n",
             operation,
             App_QueueStatusName(status),
             (unsigned long)free_count,
             (unsigned long)data_count);
  }

  App_UART_Print(text);
}
