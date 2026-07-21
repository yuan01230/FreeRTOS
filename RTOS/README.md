# FreeRTOS 队列实验工程

本目录是 STM32F407 + FreeRTOS 学习工程，当前章节主要学习 FreeRTOS 的消息队列（Message Queue）、队列操作的 timeout 行为，以及队列传递指针时的生命周期问题。

当前实验代码位于：

`LIB/App_FreeRtos/app_freertos.c`

串口用于输出实验结果，当前队列实验不再使用 LED、TFTLCD 和中文字库。

## 当前章节学习目标

通过多个可重复的硬件实验，理解：

- 队列用于任务之间传递数据。
- 队列保存的是消息副本，不是发送任务中局部变量的地址。
- 队列既可以保存结构体副本，也可以保存指针值。
- 传递指针时，必须保证指针指向的数据生命周期足够长。
- 队列有固定容量，不是无限缓存。
- 队列满时，发送任务可以立即失败、阻塞等待，或等待超时。
- 队列空时，接收任务可以立即失败、阻塞等待，或等待超时。
- 任务阻塞后不会一直占用 CPU。
- 队列中出现空间或消息后，被阻塞的任务可以重新进入 Ready 状态。
- 高优先级任务被唤醒后，可能抢占低优先级任务。
- 串口日志打印顺序不一定完全等于队列操作发生顺序。

## 工程结构

```text
RTOS/
├── Core/                         STM32CubeMX 生成的核心代码
├── Drivers/                     STM32 HAL、CMSIS 驱动
├── Middlewares/Third_Party/
│   └── FreeRTOS/                 FreeRTOS 内核和 CMSIS-RTOS2 适配层
├── LIB/
│   ├── App_FreeRtos/
│   │   ├── app_freertos.c       当前队列实验代码
│   │   └── app_freertos.h
│   └── UART/                    串口输出封装
├── CMakeLists.txt                CMake 工程文件
├── CMakePresets.json             Debug/Release 构建预设
└── RTOS.ioc                      STM32CubeMX 工程配置
```

当前实验中，应用入口函数是：

`App_FreeRTOS_Init();`

它负责创建队列和实验任务。该函数应在：

```text
osKernelInitialize() 之后
osKernelStart() 之前
```

被调用。

## 队列基本概念

实验 C 的队列创建代码：

```c
appQueueHandle =
    osMessageQueueNew(APP_QUEUE_LENGTH,
                      sizeof(AppQueueMessage_t),
                      NULL);
```

对应的配置是：

```c
#define APP_QUEUE_LENGTH 1U
```

表示队列最多保存 1 条消息。

消息结构体：

```c
typedef struct
{
  uint32_t sequence;
} AppQueueMessage_t;
```

发送消息：

```c
osMessageQueuePut(queue, &message, 0U, timeout);
```

接收消息：

```c
osMessageQueueGet(queue, &message, NULL, timeout);
```

队列会复制结构体内容，因此下面这种发送方式是安全的：

```c
AppQueueMessage_t message;
message.sequence = 1U;

osMessageQueuePut(queue, &message, 0U, 0U);
```

队列不会保存 message 局部变量的地址，而是把消息内容复制到队列内部。

实验 D 的队列创建代码：

```c
appQueueHandle =
    osMessageQueueNew(APP_QUEUE_LENGTH,
                      sizeof(AppQueueMessage_t *),
                      NULL);
```

这表示队列里每个元素只保存一个 `AppQueueMessage_t *` 指针。队列复制的是地址值，不会复制地址指向的整块结构体内容。

## timeout 参数

timeout 的单位是 tick，不一定等于毫秒。

```c
timeout = 0U;
```

不等待，条件不满足时立即返回。

```c
timeout = 500U;
```

最多等待 500 tick，条件仍不满足时返回超时。

```c
timeout = 2000U;
```

最多等待 2000 tick。

```c
timeout = osWaitForever;
```

一直等待，直到操作成功。

常见返回值：

| 返回值 | 含义 |
| --- | --- |
| `osOK` | 操作成功 |
| `osErrorResource` | timeout 为 0 时，队列满或队列空，资源当前不可用 |
| `osErrorTimeout` | 等待指定 tick 后，条件仍然没有满足 |
| `osErrorParameter` | 参数错误 |
| `osErrorISR` | 在不允许阻塞的中断上下文中调用 |

## 当前程序如何切换测试

打开：

```text
LIB/App_FreeRtos/app_freertos.c
```

### 实验 C：切换 PUT / GET 模式

如果源码使用实验 C 的 timeout 程序，PUT 模式测试“队列满时发送”：

```c
#define APP_QUEUE_TEST_MODE APP_QUEUE_TEST_PUT
```

GET 模式测试“队列空时接收”：

```c
#define APP_QUEUE_TEST_MODE APP_QUEUE_TEST_GET
```

修改后需要重新编译并烧录。

### 切换 timeout

```c
#define APP_QUEUE_WAIT_TICKS 0U
```

测试立即返回。

```c
#define APP_QUEUE_WAIT_TICKS 500U
```

测试等待 500 tick 后的结果。

```c
#define APP_QUEUE_WAIT_TICKS 2000U
```

测试等待期间条件满足后成功。

推荐顺序：

```text
PUT + timeout=0
PUT + timeout=500
PUT + timeout=2000
GET + timeout=0
GET + timeout=500
GET + timeout=2000
```

### 实验 D：当前程序

当前 `LIB/App_FreeRtos/app_freertos.c` 已切换为实验 D：队列传递指针。

当前程序不再通过 `APP_QUEUE_TEST_MODE` 切换 PUT / GET timeout 测试，而是固定运行：

```text
实验 D：生产者发送 AppQueueMessage_t *，消费者接收 AppQueueMessage_t *
```

如需回看实验 C，不需要重新做实验；本 README 已保留实验 C 的六组测试结果和分析。

## PUT 实验：队列满时的 timeout

### PUT + timeout=2000：等待后发送成功

队列长度为 1。生产者先发送 put-1 填满队列，再发送 put-2；消费者延时约 1000 tick 后取出第一条消息。

典型日志：

```text
[timeout] op=put-1 seq=1 status=osOK elapsed=0 count=1 space=0
[timeout] op=put-2 seq=1 status=osOK elapsed=1004 count=1 space=0
[timeout] op=get seq=1 status=osOK elapsed=5 count=1 space=0
```

过程：

```text
put-1 立即成功，队列变满。
put-2 发现队列已满，进入 Blocked 状态等待。
消费者约 1000 tick 后取走第一条消息。
队列出现空间，put-2 被唤醒并成功放入第二条消息。
```

put-2 的等待时间约为 1000 tick，而不是完整的 2000 tick，因为消息空间提前出现。

消费者日志中可能仍显示 count=1、space=0，因为消费者取走第一条消息后，生产者立即补入了第二条消息。

### PUT + timeout=500：等待后超时

消费者约 1000 tick 后才取消息，但生产者最多只等待 500 tick。

典型日志：

```text
[timeout] op=put-1 seq=1 status=osOK elapsed=0 count=1 space=0
[timeout] op=put-2 seq=1 status=osErrorTimeout elapsed=500 count=1 space=0
[timeout] op=get seq=1 status=osOK elapsed=0 count=0 space=1
```

过程：

```text
put-1 成功，队列变满。
put-2 阻塞等待队列空间。
500 tick 到期时，消费者还没有取出第一条消息。
put-2 返回 osErrorTimeout。
第二条消息没有进入队列，第一条消息继续保留。
消费者随后成功取出第一条消息。
```

### PUT + timeout=0：立即失败

典型日志：

```text
[timeout] op=put-1 seq=1 status=osOK elapsed=0 count=1 space=0
[timeout] op=put-2 seq=1 status=osErrorResource elapsed=0 count=1 space=0
[timeout] op=get seq=1 status=osOK elapsed=0 count=0 space=1
```

队列已满时，put-2 不阻塞，立即返回 osErrorResource。第二条消息没有进入队列，第一条消息不会因此丢失。

## GET 实验：队列空时的 timeout

### GET + timeout=0：队列空时立即失败

生产者启动后延时约 1000 tick 才发送消息，消费者不等待。

典型日志：

```text
[timeout] op=get seq=1 status=osErrorResource elapsed=0 count=0 space=1
[timeout] op=put seq=1 status=osOK elapsed=0 count=1 space=0
[timeout] op=get seq=2 status=osOK elapsed=0 count=0 space=1
```

第一次 get 时队列为空，立即返回 osErrorResource。生产者稍后发送消息，消费者下一轮接收时立即取出消息。

这里 get seq 表示消费者第几次尝试接收，put seq 表示生产者发送的消息序号，两者不一定相同。

### GET + timeout=500：等待后超时或成功

典型第一次接收日志：

```text
[timeout] op=get seq=1 status=osErrorTimeout elapsed=500 count=0 space=1
[timeout] op=put seq=1 status=osOK elapsed=0 count=1 space=0
```

消费者最多等待 500 tick，但生产者约 1000 tick 后才发送，因此第一次 get 返回 osErrorTimeout。生产者随后发送成功，消息保留在队列中，消费者下一轮可以取出。

如果消息在 500 tick 内到达，则 get 会返回 osOK。

### GET + timeout=2000：等待消息后成功

典型日志：

```text
[timeout] op=put seq=1 status=osOK elapsed=0 count=1 space=0
[timeout] op=get seq=1 status=osOK elapsed=1001 count=0 space=1

[timeout] op=put seq=2 status=osOK elapsed=0 count=1 space=0
[timeout] op=get seq=2 status=osOK elapsed=999 count=0 space=1
```

消费者先发现队列为空并进入 Blocked 状态。生产者约 1000 tick 后发送消息，消费者被唤醒并取出消息。实际等待约 1000 tick，小于最大等待时间 2000 tick。

## 六组实验对比

| 模式 | timeout | 条件 | 结果 |
| --- | ---: | --- | --- |
| PUT | 0 | 队列满 | 立即返回 osErrorResource |
| PUT | 500 | 队列满，500 tick 内无空间 | 返回 osErrorTimeout |
| PUT | 2000 | 队列满，等待期间出现空间 | 返回 osOK |
| GET | 0 | 队列空 | 立即返回 osErrorResource |
| GET | 500 | 队列空，500 tick 内无消息 | 返回 osErrorTimeout |
| GET | 2000 | 队列空，等待期间收到消息 | 返回 osOK |

## 实验 D：队列传递指针

### 实验目的

前面的实验 C 传递的是结构体副本：

```c
osMessageQueueNew(APP_QUEUE_LENGTH, sizeof(AppQueueMessage_t), NULL);
osMessageQueuePut(appQueueHandle, &message, 0U, timeout);
```

实验 D 改成传递结构体指针：

```c
osMessageQueueNew(APP_QUEUE_LENGTH, sizeof(AppQueueMessage_t *), NULL);
osMessageQueuePut(appQueueHandle, &message_ptr, 0U, 0U);
```

这两个写法很像，但含义完全不同：

| 方式 | 队列元素大小 | 队列保存的内容 |
| --- | --- | --- |
| 传结构体 | `sizeof(AppQueueMessage_t)` | 结构体内容副本 |
| 传指针 | `sizeof(AppQueueMessage_t *)` | 一个地址值 |

### 当前实验设计

程序中定义了一个静态消息对象：

```c
static AppQueueMessage_t staticMessage;
```

生产者任务每隔一段时间填写这个静态对象：

```c
staticMessage.sequence = sequence;
staticMessage.created_tick = osKernelGetTickCount();
message_ptr = &staticMessage;
```

然后把 `message_ptr` 这个指针值放入队列：

```c
osMessageQueuePut(appQueueHandle, &message_ptr, 0U, 0U);
```

消费者任务从队列取出指针：

```c
osMessageQueueGet(appQueueHandle, &message_ptr, NULL, osWaitForever);
```

消费者再通过这个指针读取消息内容：

```c
message_ptr->sequence
message_ptr->created_tick
```

### 预期串口日志

典型日志格式：

```text
[ptr] experiment D start: queue stores AppQueueMessage_t *
[ptr] expected: put_addr == get_addr == static_addr
[ptr] op=put seq=1 status=osOK put_addr=0x20000000 count=1 space=0
[ptr] op=get rx=1 status=osOK get_addr=0x20000000 static_addr=0x20000000 seq=1 tick=123 count=0 space=1
```

实际地址不一定是 `0x20000000`，要以板子输出为准。

### 实测串口结果

本次实测回显：

```text
[ptr] experiment D start: queue stores AppQueueMessage_t *
[ptr] expected: put_addr == get_addr == static_addr
[ptr] op=put seq=1 status=osOK put_addr=0x20004E78 count=1 space=0
[ptr] op=get rx=1 status=osOK get_addr=0x20004E78 static_addr=0x20004E78 seq=1 tick=0 count=0 space=1

[ptr] op=put seq=2 status=osOK put_addr=0x20004E78 count=1 space=0
[ptr] op=get rx=2 status=osOK get_addr=0x20004E78 static_addr=0x20004E78 seq=2 tick=2005 count=0 space=1

[ptr] op=put seq=3 status=osOK put_addr=0x20004E78 count=1 space=0
[ptr] op=get rx=3 status=osOK get_addr=0x20004E78 static_addr=0x20004E78 seq=3 tick=4010 count=0 space=1
```

实测结论：

- `put_addr`、`get_addr`、`static_addr` 都是 `0x20004E78`，说明队列传递的是同一个静态消息对象地址。
- `seq` 从 1 递增到 3，说明消费者通过指针读到了生产者更新后的消息内容。
- `tick` 约每 2000 tick 增加一次，与 `APP_POINTER_SEND_PERIOD_TICKS` 的配置一致。
- put 后 `count=1 space=0`，get 后 `count=0 space=1`，说明队列中保存的是一个指针元素，消费者取走后队列重新变空。

分析重点：

- `put_addr` 是生产者发送的消息对象地址。
- `get_addr` 是消费者从队列中取出的地址。
- `static_addr` 是静态消息对象的真实地址。
- 正常情况下三者应相同。
- `seq` 是消费者通过指针读到的消息序号。
- `count=0 space=1` 表示消费者取走指针后，队列重新变空。

### 为什么不能发送局部变量地址

下面这种写法不适合直接用于任务间传指针：

```c
static void Producer(void)
{
    AppQueueMessage_t message;
    AppQueueMessage_t *message_ptr = &message;

    osMessageQueuePut(queue, &message_ptr, 0U, 0U);
}
```

原因是 `message` 是局部变量。任务继续运行、函数返回，或者下一轮循环重新使用这块栈空间后，消费者拿到的地址可能已经不再代表原来的消息内容。

实验 D 使用 `staticMessage`，是因为它的生命周期覆盖整个程序运行过程。这样可以先安全观察“队列只保存指针值”的行为。

### 实验 D 的局限

当前实验只有一个静态消息对象，适合学习指针传递的基本原理。

如果生产者发送很快，消费者处理很慢，单个静态对象可能在消费者使用前被生产者覆盖。真实项目中更推荐继续升级为实验 E：

```text
freeQueue 保存空闲消息块指针
dataQueue 保存待处理消息块指针
```

也就是用静态消息池管理多个消息对象，确保每个消息对象同一时间只被一个任务拥有。

## 任务状态变化

### PUT 模式

```text
生产者发送第一条消息
    ↓
队列变满
    ↓
生产者发送第二条消息
    ↓
生产者进入 Blocked，等待队列空间
    ↓
消费者取走第一条消息
    ↓
队列出现空间
    ↓
生产者变为 Ready
    ↓
生产者继续发送第二条消息
```

### GET 模式

```text
消费者尝试接收
    ↓
队列为空
    ↓
消费者进入 Blocked，等待消息
    ↓
生产者发送消息
    ↓
消费者变为 Ready
    ↓
消费者接收消息并继续运行
```

## 串口日志说明

当前格式：

```text
[timeout] op=put-2 seq=1 status=osErrorTimeout elapsed=500 count=1 space=0
```

| 字段 | 含义 |
| --- | --- |
| `op` | 操作类型，put-1、put-2 或 get |
| `seq` | 消息序号或接收尝试序号 |
| `status` | CMSIS-RTOS2 API 返回值 |
| `elapsed` | 本次 API 调用实际等待的 tick 数 |
| `count` | 当前队列中的消息数量 |
| `space` | 当前队列剩余空间 |

日志顺序可能受到任务抢占影响，因此日志先打印 put 不一定表示消费者之后才开始等待。分析时要结合 status、elapsed、count、space 一起判断。

## 从实验中得到的设计原则

### 队列不是无限缓存

如果长期满足：

```text
生产速度 > 消费速度
```

队列最终会变满。可选策略包括：

- 丢弃新消息；
- 阻塞等待；
- 增大队列；
- 降低生产频率；
- 覆盖旧消息；
- 将不同优先级消息拆分到不同队列。

### 必须检查 API 返回值

发送和接收都不能忽略返回值：

```c
osStatus_t status;
status = osMessageQueuePut(queue, &message, 0U, timeout);

if (status != osOK)
{
  // 根据业务处理失败
}
```

### 队列传值和传指针不同

实验 C 传递的是结构体副本，实验 D 传递的是指针。传递指针时必须保证：

- 不能发送局部变量地址；
- 指针指向的数据生命周期足够长；
- 消费者使用完数据前，数据不能被释放或覆盖；
- 必要时使用静态缓冲区、内存池或消息对象复用。

## 构建与验证

工程提供 Debug 和 Release 构建预设：

```powershell
cmake --preset Debug
cmake --build --preset Debug
```

修改测试宏后的验证流程：

```text
1. 修改 APP_QUEUE_TEST_MODE。
2. 修改 APP_QUEUE_WAIT_TICKS。
3. 编译工程。
4. 烧录 STM32F407。
5. 打开串口。
6. 观察 status、elapsed、count、space。
7. 分析任务是否阻塞、唤醒和重新运行。
```

实验 D 的验证流程：

```text
1. 编译工程。
2. 烧录 STM32F407。
3. 打开串口。
4. 观察 put_addr、get_addr、static_addr。
5. 确认三个地址一致。
6. 观察消费者是否能通过指针读到正确的 seq 和 tick。
```

## 当前章节后续方向

完成队列 timeout 和指针传递实验后，建议继续学习：

1. 静态消息池和内存池；
2. 双队列管理消息块所有权；
3. 中断中发送队列消息；
4. xQueueSendFromISR() 与 CMSIS-RTOS2 ISR 限制；
5. 队列、信号量、互斥量、事件标志和任务通知的区别；
6. Stream Buffer/Ring Buffer 处理连续串口字节流。

## 总结

队列不仅是存放消息的数组，也是任务之间进行数据传递和同步等待的机制。timeout 决定任务在资源不可用时是立即失败、等待一段时间，还是一直阻塞。传递指针时，队列只负责搬运地址，消息对象的生命周期和所有权需要由程序自己保证。
