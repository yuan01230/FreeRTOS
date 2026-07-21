/*
 * ============================================================================
 *                    FreeRTOS 队列实验 D：传递指针
 * ============================================================================
 *
 * 【当前程序测试内容】
 *
 * 本实验观察“队列传值”和“队列传指针”的区别。
 *
 * 实验 C 中，队列元素大小是 sizeof(AppQueueMessage_t)，队列保存的是
 * 结构体内容副本。
 *
 * 当前实验 D 中，队列元素大小是 sizeof(AppQueueMessage_t *)，队列保存的是
 * 指针变量的值，也就是一个地址。消费者通过这个地址访问生产者准备好的
 * 静态消息对象。
 *
 * 【本实验要验证什么】
 *
 *   1. 生产者发送的是消息对象地址；
 *   2. 消费者接收到的地址与生产者发送的地址一致；
 *   3. 队列本身只复制“指针值”，不会复制指针指向的整块数据；
 *   4. 指针指向的数据必须在消费者使用期间一直有效。
 *
 * 【如何切换不同测试】
 *
 * 当前文件只运行实验 D。前面实验 C 的 timeout 测试结果已经整理到
 * RTOS/README.md 中。
 *
 * 后续如果要继续做实验 E，可以在本文件基础上增加“静态消息池”：
 *   - freeQueue：保存空闲消息块指针；
 *   - dataQueue：保存待处理消息块指针。
 *
 * 【重要提醒】
 *
 * 不要把局部变量地址放进队列后让任务继续运行或函数返回。
 * 局部变量的生命周期不稳定，消费者拿到的指针可能已经失效。
 *
 * 本实验使用 static 全局消息对象，生命周期覆盖整个程序运行过程，
 * 因此适合用来安全观察“队列传指针”的基本行为。
 *
 * ============================================================================
 */

#include "app_freertos.h"
#include "../UART/app_uart.h"
#include "cmsis_os.h"
#include <stdint.h>
#include <stdio.h>

/*
 * 队列长度仍然设置为 1。
 *
 * 这样串口日志更容易分析：
 *   - 生产者每次只放入一个消息指针；
 *   - 消费者取走后，队列重新变空；
 *   - count 和 space 的变化非常直观。
 */
#define APP_QUEUE_LENGTH              1U

/*
 * 生产者发送两次消息之间的间隔。
 * 间隔稍微拉长，可以让串口日志一轮一轮地输出，方便观察。
 */
#define APP_POINTER_SEND_PERIOD_TICKS 2000U

typedef struct
{
  /*
   * sequence 用来区分第几条消息。
   * 消费者收到指针后，会通过指针读取这个字段。
   */
  uint32_t sequence;

  /*
   * created_tick 记录生产者填写消息时的系统 tick。
   * 它不是队列 API 必须字段，只是帮助我们确认消费者读到的是同一块数据。
   */
  uint32_t created_tick;
} AppQueueMessage_t;

static void App_QueueProducerTask(void *argument);
static void App_QueueConsumerTask(void *argument);
static const char *App_QueueStatusName(osStatus_t status);
static void App_QueuePrintPut(uint32_t sequence, const AppQueueMessage_t *message,
                              osStatus_t status);
static void App_QueuePrintGet(uint32_t receive_count, const AppQueueMessage_t *message,
                              osStatus_t status);

/* 队列句柄：本实验的队列元素类型是 AppQueueMessage_t *。 */
static osMessageQueueId_t appQueueHandle;

/*
 * 静态消息对象。
 *
 * 这是本实验最关键的对象：
 *   - 生产者不会把整个结构体复制进队列；
 *   - 生产者只把 &staticMessage 这个地址放进队列；
 *   - 消费者收到地址后，再通过这个地址读取 sequence 和 created_tick。
 *
 * 因为它是 static 全局变量，所以生命周期从程序启动持续到程序结束。
 */
static AppQueueMessage_t staticMessage;

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
   * 注意第二个参数：
   *
   *   sizeof(AppQueueMessage_t *)
   *
   * 这表示队列里每个元素只保存“一个指针大小”的数据。
   * 如果这里写成 sizeof(AppQueueMessage_t)，就又变成实验 C 的结构体传值了。
   */
  appQueueHandle = osMessageQueueNew(APP_QUEUE_LENGTH,
                                     sizeof(AppQueueMessage_t *),
                                     NULL);
  if (appQueueHandle == NULL)
  {
    App_UART_Print("[ptr] queue create failed\r\n");
    return;
  }

  App_UART_Print("\r\n[ptr] experiment D start: queue stores AppQueueMessage_t *\r\n");
  App_UART_Print("[ptr] expected: put_addr == get_addr == static_addr\r\n");

  osThreadNew(App_QueueProducerTask, NULL, &queueProducerTaskAttributes);
  osThreadNew(App_QueueConsumerTask, NULL, &queueConsumerTaskAttributes);
}

/*
 * 生产者任务：填写静态消息对象，然后把对象地址发送到队列。
 *
 * 这里有一个容易看错的细节：
 *
 *   AppQueueMessage_t *message_ptr = &staticMessage;
 *   osMessageQueuePut(appQueueHandle, &message_ptr, 0U, 0U);
 *
 * message_ptr 本身是一个局部变量，但队列复制的是 message_ptr 里面保存的地址值。
 * 只要这个地址指向的 staticMessage 仍然有效，消费者就可以安全访问。
 */
static void App_QueueProducerTask(void *argument)
{
  AppQueueMessage_t *message_ptr;
  uint32_t sequence = 0U;
  osStatus_t status;

  (void)argument;

  for (;;)
  {
    sequence++;

    staticMessage.sequence = sequence;
    staticMessage.created_tick = osKernelGetTickCount();
    message_ptr = &staticMessage;

    /*
     * timeout=0：
     *   队列有空间就立即发送；
     *   队列满则立即返回 osErrorResource。
     *
     * 当前生产周期较慢，消费者一直阻塞等待，所以正常情况下应返回 osOK。
     */
    status = osMessageQueuePut(appQueueHandle, &message_ptr, 0U, 0U);
    App_QueuePrintPut(sequence, message_ptr, status);

    osDelay(APP_POINTER_SEND_PERIOD_TICKS);
  }
}

/*
 * 消费者任务：从队列中取出一个指针，再通过指针读取消息内容。
 *
 * osWaitForever 表示消费者没有消息时一直阻塞等待。
 * 阻塞期间任务不会一直占用 CPU。
 */
static void App_QueueConsumerTask(void *argument)
{
  AppQueueMessage_t *message_ptr = NULL;
  uint32_t receive_count = 0U;
  osStatus_t status;

  (void)argument;

  for (;;)
  {
    message_ptr = NULL;
    status = osMessageQueueGet(appQueueHandle, &message_ptr, NULL, osWaitForever);
    receive_count++;

    App_QueuePrintGet(receive_count, message_ptr, status);
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

static void App_QueuePrintPut(uint32_t sequence, const AppQueueMessage_t *message,
                              osStatus_t status)
{
  char text[192];
  uint32_t count = osMessageQueueGetCount(appQueueHandle);
  uint32_t space = osMessageQueueGetSpace(appQueueHandle);

  snprintf(text, sizeof(text),
           "[ptr] op=put seq=%lu status=%s put_addr=0x%08lX count=%lu space=%lu\r\n",
           (unsigned long)sequence,
           App_QueueStatusName(status),
           (unsigned long)(uintptr_t)message,
           (unsigned long)count,
           (unsigned long)space);
  App_UART_Print(text);
}

static void App_QueuePrintGet(uint32_t receive_count, const AppQueueMessage_t *message,
                              osStatus_t status)
{
  char text[224];
  uint32_t count = osMessageQueueGetCount(appQueueHandle);
  uint32_t space = osMessageQueueGetSpace(appQueueHandle);

  if ((status == osOK) && (message != NULL))
  {
    snprintf(text, sizeof(text),
             "[ptr] op=get rx=%lu status=%s get_addr=0x%08lX static_addr=0x%08lX seq=%lu tick=%lu count=%lu space=%lu\r\n",
             (unsigned long)receive_count,
             App_QueueStatusName(status),
             (unsigned long)(uintptr_t)message,
             (unsigned long)(uintptr_t)&staticMessage,
             (unsigned long)message->sequence,
             (unsigned long)message->created_tick,
             (unsigned long)count,
             (unsigned long)space);
  }
  else
  {
    snprintf(text, sizeof(text),
             "[ptr] op=get rx=%lu status=%s get_addr=0x%08lX count=%lu space=%lu\r\n",
             (unsigned long)receive_count,
             App_QueueStatusName(status),
             (unsigned long)(uintptr_t)message,
             (unsigned long)count,
             (unsigned long)space);
  }

  App_UART_Print(text);
}
