# FreeRTOS 学习日记

这个文件用于记录 FreeRTOS 学习过程中的实验、问答、理解修正和 Git 提交节点。每完成一个学习进度，就在这里追加一篇日记。

## 2026-07-01：第一天，任务基础

### 今日学习主题

- 理解 FreeRTOS 最核心概念：任务、调度、延时、优先级。
- 看懂 `osThreadNew()` 如何创建任务。
- 理解 `osDelay()` 是让出 CPU，不是裸机里的死等延时。

### 今日实验进度

第一天的任务实验功能已经完成。

当前重点从“功能能不能跑”转向“我是否真的理解它为什么能跑”。

### 问题 1：`ledTask` 调用 `osDelay(500)` 后，它去哪了？

我的回答：

```text
ledTask 调用 osDelay(500) 后，任务进入阻塞态，时间长度是 500 个 tick，把 CPU 使用权限让出来。
```

修正和理解：

这个理解是对的。更准确地说：

```text
ledTask 进入阻塞态，阻塞时间是 500 个 RTOS tick。
```

如果 `configTICK_RATE_HZ = 1000`，那么 500 tick 就是 500 ms。

如果 tick 频率不是 1000 Hz，那么 `osDelay(500)` 就不一定等于 500 ms。

计算方式：

```text
实际延时时间 = delay_ticks / configTICK_RATE_HZ 秒
```

例如：

```text
configTICK_RATE_HZ = 1000，osDelay(500) = 0.5 秒
configTICK_RATE_HZ = 100，osDelay(500) = 5 秒
```

关键理解：

```text
osDelay() 阻塞的是当前任务，不是让整个 CPU 停住。
```

### 问题 2：`uartTask` 和 `ledTask` 同时就绪时，谁先运行？

我的回答：

```text
谁的优先级高，谁先运行。高优先级可以打断低优先级的运行状态。
```

修正和理解：

这个理解是对的。

在 FreeRTOS 开启抢占式调度时：

```text
如果低优先级任务正在运行，
此时高优先级任务变为就绪态，
FreeRTOS 会立即切换到高优先级任务。
```

这就是抢占。

例子：

```text
ledTask 正在运行，优先级 Low。
uartTask 的 osDelay() 到期，变为就绪态，优先级 Normal。
因为 uartTask 优先级更高，所以 uartTask 会抢占 ledTask。
```

关键理解：

```text
多个任务同时就绪时，优先级高的任务先运行。
高优先级任务从阻塞态恢复为就绪态时，可以抢占低优先级任务。
```

### 问题 3：如果高优先级任务的 `while (1)` 里没有 `osDelay()`，会发生什么？

我的回答：

```text
高优先级任务会持续获取 CPU 使用权限，低优先级无法得到响应，一直处于就绪态饿死。
```

修正和理解：

这个理解很准确。

高优先级任务如果一直不阻塞、不延时、不等待事件，那么它会一直保持就绪态或运行态。

结果是：

```text
低优先级任务即使已经就绪，也没有机会运行。
```

这种情况可以叫：

```text
低优先级任务被饥饿。
```

错误写法：

```c
while (1)
{
    HAL_UART_Transmit(...);
}
```

正确写法：

```c
while (1)
{
    HAL_UART_Transmit(...);
    osDelay(1000);
}
```

除了 `osDelay()`，下面这些等待操作也可以让任务进入阻塞态：

```text
osMessageQueueGet()
osSemaphoreAcquire()
osEventFlagsWait()
```

关键理解：

```text
RTOS 任务不能一直霸占 CPU。
任务完成一小段工作后，应该主动延时、阻塞或等待事件。
```

### 问题 4：`osThreadNew()` 里的 `stack_size` 和 `priority` 分别控制什么？

我的回答：

```text
stack_size 控制任务栈的空间大小。
priority 控制任务优先级大小。
```

修正和理解：

这个理解是对的。

典型任务属性：

```c
static const osThreadAttr_t ledTaskAttributes = {
    .name = "ledTask",
    .stack_size = 128 * 4,
    .priority = osPriorityLow,
};
```

含义：

```text
name        任务名字，方便调试
stack_size  任务自己的栈空间大小
priority    任务参与调度时的优先级
```

需要注意：

```text
任务栈太小：可能栈溢出、HardFault、程序跑飞。
任务栈太大：浪费 RAM。
```

后面学习调试时，需要用下面的 API 查看任务栈还剩多少：

```c
uxTaskGetStackHighWaterMark()
```

### 今日核心结论

今天最重要的三句话：

```text
任务调用 osDelay() 后，不是 CPU 停住，而是当前任务阻塞。
高优先级任务就绪时，可以抢占低优先级任务。
高优先级任务如果永不阻塞，低优先级任务可能永远运行不了。
```

### 今日掌握情况

已经掌握：

- 任务会在运行态、就绪态、阻塞态之间切换。
- `osDelay()` 会让当前任务进入阻塞态。
- 高优先级任务可以抢占低优先级任务。
- 高优先级任务如果不让出 CPU，会导致低优先级任务饥饿。
- `osThreadNew()` 通过任务函数、参数和任务属性创建任务。
- `stack_size` 和 `priority` 是任务创建时最重要的属性之一。

还需要继续观察：

- 当前工程中 `configTICK_RATE_HZ` 的具体值是多少。
- 当前任务的栈大小是否合适。
- 不同优先级组合下，LED 和 UART 的实际运行现象是否符合预期。

### 下一步

进入 Git 管理流程：

```text
1. git status
2. git diff
3. 确认本次 FreeRTOS 任务基础实验改了哪些文件
4. 编译验证
5. 提交本次学习进度
```

推荐提交信息：

```text
learn: add FreeRTOS task basic demo
```

## 2026-07-01：第二天，任务状态和调度

### 今日学习主题

- 理解 FreeRTOS 任务的 `Running`、`Ready`、`Blocked` 三种常见状态。
- 理解 `osDelay()` 为什么会让任务进入阻塞态。
- 理解高优先级任务什么时候会抢占低优先级任务。
- 理解同优先级任务进行时间片轮转的前提。

### 今日实验方向

今天的实验重点不是新增复杂功能，而是观察调度现象：

```text
1. 让 ledTask 和 uartTask 使用不同优先级，观察高优先级任务是否能抢占低优先级任务。
2. 临时让高优先级任务不调用 osDelay()，观察低优先级任务是否被饿死。
3. 让两个任务使用相同优先级，观察在时间片调度开启时是否都能运行。
```

实验时要注意：

```text
不带 osDelay() 的死循环只用于短时间观察现象，实验结束后必须改回。
```

### 问题 1：`Running`、`Ready`、`Blocked` 三种状态分别是什么意思？

我的回答整理：

```text
Running 是运行态，表示任务正在使用 CPU。
Ready 是就绪态，表示任务已经准备好，正在等待获得 CPU 使用权。
Blocked 是阻塞态，表示任务暂时放弃 CPU 使用权，等待某个条件满足。
```

进一步理解：

FreeRTOS 调度时，会从就绪态任务中选择最合适的任务运行。

如果当前运行任务是 A，就绪任务是 B：

```text
A 优先级 > B 优先级：
    A 继续运行，直到 A 阻塞、延时、挂起或被更高优先级任务抢占。

A 优先级 = B 优先级：
    如果开启时间片轮转，并且两个任务都处于 Ready 状态，
    A 和 B 可以轮流获得 CPU。

A 优先级 < B 优先级：
    如果开启抢占式调度，B 会抢占 A，
    B 进入 Running，A 回到 Ready。
```

如果不开启抢占式调度，则高优先级任务即使已经就绪，也不一定立刻抢占当前任务，要等当前任务主动让出 CPU 或发生调度点。

术语修正：

```text
Blocked 不只是“放弃 CPU”，而是“因为等待某个条件，暂时不能被调度运行”。
```

常见进入 `Blocked` 的原因：

```text
等待延时时间到期：osDelay()
等待队列消息：osMessageQueueGet()
等待信号量：osSemaphoreAcquire()
等待事件标志：osEventFlagsWait()
```

### 问题 2：为什么 `osDelay()` 会让任务进入 `Blocked`？

我的回答整理：

```text
运行 osDelay() 后，当前任务会让出 CPU，并进入阻塞态。
```

修正后的准确理解：

`osDelay()` 的核心不是“压栈”，也不是“挂起任务”，而是：

```text
把当前任务从 Running 状态移入延时阻塞列表。
等待指定 tick 数到期后，再把任务移回 Ready 状态。
```

例如：

```c
osDelay(500);
```

可以理解为：

```text
当前任务告诉调度器：
500 个 tick 以内不要调度我。
时间到以后，我再回到 Ready 状态，等待下一次调度。
```

任务切换时，CPU 上下文确实会被保存，例如寄存器、栈指针等，但这是任务切换机制的一部分，不是 `osDelay()` 这个 API 最核心的含义。

需要特别区分：

```text
Blocked：
    等待条件，条件满足后可以自动回到 Ready。

Suspended：
    被人为挂起，不会自己恢复，必须由 osThreadResume() 或 vTaskResume() 恢复。
```

所以今天要修正的关键术语是：

```text
osDelay() 让任务进入延时阻塞态，不是把任务挂起。
```

### 问题 3：高优先级任务什么时候会抢占低优先级任务？

我的回答整理：

```text
在开启任务抢占功能时，高优先级任务进入就绪态后，会抢占正在运行的低优先级任务。
```

这个理解是正确的。

更完整的条件是：

```text
1. FreeRTOS 开启抢占式调度，configUSE_PREEMPTION = 1。
2. 高优先级任务从 Blocked、Suspended 或其他状态变成 Ready。
3. 高优先级任务的优先级高于当前 Running 任务。
```

满足这些条件时，调度器会切换任务：

```text
低优先级任务：Running -> Ready
高优先级任务：Ready -> Running
```

常见抢占场景：

```text
高优先级任务 osDelay() 到期。
高优先级任务等待的队列收到了消息。
高优先级任务等待的信号量被释放。
中断唤醒了高优先级任务。
```

实验观察点：

```text
如果 uartTask 优先级高于 ledTask，
当 uartTask 延时时间到期并进入 Ready，
它会优先获得 CPU。
```

### 问题 4：同优先级任务是不是一定公平轮流运行？前提是什么？

我的回答整理：

```text
只有在开启时间片轮转的情况下，同一优先级的任务才会进行轮流运行。
```

这个方向是对的，但还需要补充几个前提。

同优先级任务想要轮流运行，通常需要：

```text
1. 开启时间片调度，configUSE_TIME_SLICING = 1。
2. 多个同优先级任务都处于 Ready 状态。
3. 没有更高优先级任务一直占用 CPU。
```

所以更准确的结论是：

```text
同优先级任务不是一定公平轮流运行。
只有在时间片调度开启，并且多个同优先级任务都持续处于 Ready 状态时，
它们才会按 tick 进行时间片轮转。
```

如果其中一个任务调用了：

```c
osDelay(1000);
```

那么它会进入 `Blocked`，在这段时间里不参与时间片轮转。

如果有更高优先级任务一直不阻塞，那么同优先级的低一层任务也没有机会运行。

### 今日核心结论

今天最重要的几句话：

```text
Running 表示任务正在运行，Ready 表示任务准备好了但还没拿到 CPU。
Blocked 表示任务在等待条件，条件满足后才能回到 Ready。
osDelay() 会让当前任务进入延时阻塞态，不是挂起态。
开启抢占式调度时，高优先级任务变为 Ready 后可以抢占低优先级任务。
同优先级任务要轮流运行，需要开启时间片调度，并且多个任务都处于 Ready。
```

### 今日掌握情况

已经掌握：

- 能区分 `Running`、`Ready`、`Blocked`。
- 能用优先级关系解释任务 A 和任务 B 谁会运行。
- 理解高优先级任务变为 `Ready` 后可以抢占低优先级任务。
- 理解同优先级时间片轮转不是无条件发生的。
- 知道 `osDelay()` 进入的是延时阻塞态，不是挂起态。

还需要继续观察：

- 当前工程是否开启 `configUSE_PREEMPTION`。
- 当前工程是否开启 `configUSE_TIME_SLICING`。
- 同优先级任务在都不阻塞时，串口或 LED 现象是否能体现时间片轮转。
- 高优先级任务不调用 `osDelay()` 时，低优先级任务是否会被饿死。

### 下一步

建议在工程里查找下面两个配置：

```text
configUSE_PREEMPTION
configUSE_TIME_SLICING
```

然后结合实验现象回答：

```text
当前工程是不是抢占式调度？
当前工程是否允许同优先级任务时间片轮转？
```

### 三个调度实验记录

#### 实验 1：抢占实验

实验配置：

```text
APP_SCHEDULER_EXPERIMENT = 1
uartTask 优先级高于 LED 任务。
LED0 和 LED1 都会周期性 osDelay()。
uartTask 每 1000ms 打印一次 tick。
```

串口现象：

```text
[exp1 preemption] tick=525618, uart priority > led priority
[exp1 preemption] tick=526623, uart priority > led priority
[exp1 preemption] tick=527628, uart priority > led priority
```

板上现象：

```text
串口周期打印。
LED0 周期闪烁。
LED1 周期闪烁。
```

现象分析：

`uartTask` 优先级高于 LED 任务，但它每次打印后都会调用 `osDelay(1000)` 进入阻塞态。

所以调度过程可以理解为：

```text
uartTask 运行 -> 打印 -> osDelay() -> 进入 Blocked
LED 任务获得 CPU -> LED 闪烁 -> osDelay() -> 进入 Blocked
uartTask 延时到期 -> 回到 Ready -> 抢占低优先级任务或优先运行
```

结论：

```text
高优先级任务不会一直霸占 CPU。
只要高优先级任务会阻塞或延时，低优先级任务仍然可以运行。
```

#### 实验 2：高优先级任务不让出 CPU

实验配置：

```text
APP_SCHEDULER_EXPERIMENT = 2
uartTask 优先级高于 LED 任务。
uartTask 不调用 osDelay()。
```

串口现象：

```text
FreeRTOS scheduler experiment 2 start.
```

之后串口不再继续打印。

板上现象：

```text
LED0 不闪烁。
LED1 不闪烁。
```

现象分析：

`uartTask` 是高优先级任务，并且进入 `for (;;)` 后不调用 `osDelay()`，也不等待队列、信号量或事件。

因此它一直保持 `Running` 或 `Ready`，低优先级 LED 任务没有机会获得 CPU。

调度现象可以理解为：

```text
uartTask 一直可运行
LED0/LED1 虽然已经 Ready
但优先级低，无法抢占 uartTask
LED0/LED1 被饿死
```

结论：

```text
高优先级任务如果永不阻塞，低优先级任务会被饿死。
这正是 FreeRTOS 任务中不能随便写无延时死循环的原因。
```

注意：

```text
这个实验只适合短时间观察。
实验结束后必须切回安全模式，例如 APP_SCHEDULER_EXPERIMENT = 1。
```

#### 实验 3：同优先级时间片实验

实验配置：

```text
APP_SCHEDULER_EXPERIMENT = 3
LED0 和 LED1 任务设置为同优先级。
LED0 和 LED1 任务不主动 osDelay()，持续保持 Ready。
uartTask 周期性打印两个 LED 任务的循环计数。
```

串口现象：

```text
FreeRTOS scheduler experiment 3 start.
[exp3 time-slice] tick=3, led0=0, led1=0
[exp3 time-slice] tick=1007, led0=3321730, led1=3335905
[exp3 time-slice] tick=2011, led0=6639107, led1=6671804
[exp3 time-slice] tick=3015, led0=9956501, led1=10007703
[exp3 time-slice] tick=4020, led0=13280029, led1=13343607
[exp3 time-slice] tick=5025, led0=16602943, led1=16679504
```

板上现象：

```text
串口周期打印。
LED0 和 LED1 高频闪烁。
两个计数值都持续增加，并且数量级接近。
```

现象分析：

LED0 和 LED1 是同优先级任务，并且都持续处于 `Ready` 状态。

从串口计数可以看到：

```text
led0 和 led1 都在持续运行。
两个计数值增长速度接近。
```

这说明调度器没有只运行其中一个任务，而是在两个同优先级任务之间切换。

这里要把对象说准确：

```text
共享时间片的不是 LED0 和 LED1 这两个硬件外设，
而是控制 LED0 的 led0Task 和控制 LED1 的 led1Task。
```

也就是说：

```text
led0Task 和 led1Task 优先级相同。
两个任务都不调用 osDelay()。
两个任务都长期处于 Ready 状态。
调度器在这两个同优先级 Ready 任务之间进行时间片轮转。
```

串口里的 `led0`、`led1` 计数值本质上记录的是两个任务循环运行的次数，不是 LED 硬件本身的状态次数。

实验 3 开头出现：

```text
FreeRTOS scheduler experiment
3 start.
```

被拆成两次接收，这是串口接收工具显示或底层发送分片造成的现象，不影响调度实验结论。

结论：

```text
当多个同优先级任务都处于 Ready 状态时，如果系统启用时间片调度，它们可以轮流获得 CPU。
从 led0 和 led1 的计数都持续增长可以看出，两个任务都获得了运行机会。
更准确地说，是 led0Task 和 led1Task 两个任务共享同一优先级下的 CPU 时间片。
```

### 三个实验总对比

| 实验 | 关键条件 | 观察现象 | 说明 |
| --- | --- | --- | --- |
| 实验 1 | 高优先级任务会 `osDelay()` | 串口和 LED 都正常运行 | 高优先级任务阻塞后，低优先级任务可以运行 |
| 实验 2 | 高优先级任务不阻塞 | 只打印启动信息，LED 不闪 | 低优先级任务被饿死 |
| 实验 3 | 两个同优先级任务持续 Ready | 两个计数都增长，LED 高频闪烁 | 同优先级任务发生时间片轮转 |

今天从实验得到的最终结论：

```text
调度器永远优先选择最高优先级的 Ready 任务。
高优先级任务如果会阻塞，低优先级任务仍然有机会运行。
高优先级任务如果永不阻塞，低优先级任务会被饿死。
同优先级任务想要轮流运行，需要它们都处于 Ready，并且系统支持时间片调度。
```

推荐提交信息：

```text
docs: add FreeRTOS day2 scheduler notes
```

## 2026-07-12：第三天，任务通信：队列

### 第 8 条：结合 TFTLCD 的实验模型

原本的队列实验模型是：

```text
producerTask 产生 LED 控制消息
ledControlTask 从队列取消息并控制 LED
uartTask 打印实验状态
```

现在工程已经加入 TFTLCD、字库、EN25Q128、FatFs 等模块，所以第三天队列实验更适合改成：

```text
多个任务产生“显示消息”
通过队列发送给 lcdTask
lcdTask 统一从队列接收消息并刷新 TFTLCD
```

核心原则：

```text
不要让多个任务同时直接操作 TFTLCD。
TFTLCD 属于共享外设，刷屏函数通常耗时较长，也不适合被多个任务交叉调用。
先用一个 lcdTask 统一管理屏幕，其他任务只负责发送显示请求。
```

### 新实验模型

任务设计：

```text
lcdTask：
    初始化 TFTLCD。
    初始化字库。
    创建或等待显示消息队列。
    阻塞等待队列消息。
    收到消息后，统一刷新 LCD 指定区域。

producerTask：
    每 1000ms 产生一条显示消息。
    消息内容包含 sequence、tick、显示行号、文本内容。
    调用 osMessageQueuePut() 发送给 lcdTask。

uartTask：
    周期性打印队列状态。
    可以打印当前 sequence、队列剩余空间、队列中消息数量。

ledTask：
    保留 LED 周期闪烁，用来证明其它任务仍在运行。
```

数据流：

```text
producerTask
    |
    | osMessageQueuePut()
    v
displayQueue
    |
    | osMessageQueueGet()
    v
lcdTask
    |
    v
TFTLCD 显示
```

### 消息结构建议

队列里传递的不是字符串指针，而是一个固定大小的结构体副本：

```c
typedef enum
{
    APP_LCD_MSG_STATUS = 0,
    APP_LCD_MSG_TICK,
    APP_LCD_MSG_LED,
    APP_LCD_MSG_ERROR,
} AppLcdMessageType_t;

typedef struct
{
    AppLcdMessageType_t type;
    uint32_t sequence;
    uint32_t tick;
    uint16_t x;
    uint16_t y;
    uint16_t color;
    char text[48];
} AppLcdMessage_t;
```

这样设计的原因：

```text
1. 队列发送时会复制整个 AppLcdMessage_t。
2. text[48] 是结构体内部数组，不依赖发送任务的局部变量地址。
3. lcdTask 收到的是一份完整消息副本，可以安全显示。
4. type 字段用于区分状态消息、tick 消息、LED 消息、错误消息。
```

不建议这样做：

```c
typedef struct
{
    const char *text;
} AppLcdMessage_t;
```

原因：

```text
如果 text 指向发送任务里的局部数组，发送任务循环下一次修改或函数返回后，
lcdTask 可能读到已经变化或失效的数据。
```

### 队列创建建议

```c
static osMessageQueueId_t lcdQueueHandle;

lcdQueueHandle = osMessageQueueNew(8, sizeof(AppLcdMessage_t), NULL);
```

含义：

```text
8：队列最多缓存 8 条 LCD 显示消息。
sizeof(AppLcdMessage_t)：每条消息固定大小。
NULL：使用默认队列属性。
```

### 发送任务模型

```c
static void App_Producer_Task(void *argument)
{
    AppLcdMessage_t msg;
    uint32_t sequence = 0;

    (void)argument;

    for (;;)
    {
        sequence++;
        msg.type = APP_LCD_MSG_TICK;
        msg.sequence = sequence;
        msg.tick = osKernelGetTickCount();
        msg.x = 16;
        msg.y = 164;
        msg.color = BLACK;
        snprintf(msg.text, sizeof(msg.text), "seq=%lu tick=%lu",
                 (unsigned long)msg.sequence,
                 (unsigned long)msg.tick);

        (void)osMessageQueuePut(lcdQueueHandle, &msg, 0U, 0U);
        osDelay(1000);
    }
}
```

这里要观察：

```text
producerTask 每 1000ms 发送一次消息。
如果 lcdQueue 满了，timeout = 0 时发送会立即失败。
后续实验可以专门检查 osMessageQueuePut() 的返回值。
```

### LCD 接收任务模型

```c
static void App_LCD_Task(void *argument)
{
    AppLcdMessage_t msg;

    (void)argument;

    TFTLCD_Init();
    LCD_Clear(WHITE);

    for (;;)
    {
        if (osMessageQueueGet(lcdQueueHandle, &msg, NULL, osWaitForever) == osOK)
        {
            FRONT_COLOR = msg.color;
            LCD_Fill(msg.x, msg.y, 300, msg.y + 20, WHITE);
            LCD_ShowString(msg.x, msg.y, 280, 20, 16, (uint8_t *)msg.text);
        }
    }
}
```

这里要观察：

```text
队列为空时，lcdTask 阻塞在 osMessageQueueGet()。
lcdTask 阻塞期间不会浪费 CPU。
producerTask 放入消息后，lcdTask 被唤醒并刷新 TFTLCD。
```

### 为什么 TFTLCD 适合做队列实验

TFTLCD 比 LED 更适合观察队列，因为它能直接显示消息内容：

```text
sequence 是否递增
tick 是否变化
消息类型是什么
队列是否阻塞等待
队列满时是否丢消息
```

但 TFTLCD 也更需要注意共享资源问题：

```text
LCD_Draw、LCD_ShowString、LCD_Fill 等函数都在操作同一个屏幕和同一组总线。
如果多个任务同时调用这些函数，显示内容可能交叉、闪烁或覆盖。
```

所以第三天先采用最稳的结构：

```text
多个生产者任务可以发消息。
只有一个 lcdTask 真正操作 TFTLCD。
```

### 第三天实验目标

完成后应该能回答：

```text
1. 为什么 LCD 显示不建议由多个任务直接同时操作？
2. 为什么队列里应该传 AppLcdMessage_t 副本，而不是局部字符串指针？
3. lcdTask 阻塞等待队列时，CPU 是否还能运行其它任务？
4. 如果 producerTask 发送太快，lcdQueue 满了会发生什么？
```

### 第三天实验结果记录

串口启动阶段输出：

```text
[font] Init Flash... 0%
[flash] id=0x6817 sr=0x00
[font] Check Font... 0%
[font] Font Ready 100%

FreeRTOS queue TFTLCD demo start.
```

说明：

```text
EN25Q128 Flash 能正常读取到 ID。
字库检查通过，Font Ready 100%。
TFTLCD 队列实验任务启动成功。
```

队列运行阶段串口输出：

```text
[queue] tick=600 count=1 space=7 drops=0
[queue] tick=1604 count=0 space=8 drops=0
[queue] tick=2607 count=0 space=8 drops=0
[queue] tick=3610 count=0 space=8 drops=0
[queue] tick=4613 count=0 space=8 drops=0
[queue] tick=5616 count=0 space=8 drops=0
[queue] tick=6619 count=0 space=8 drops=0
[queue] tick=7622 count=0 space=8 drops=0
[queue] tick=8625 count=0 space=8 drops=0
[queue] tick=9628 count=0 space=8 drops=0
```

LCD 屏幕现象：

```text
LCD 屏幕动态刷新 seq=xx，tick=xxxx。
seq 和 tick 都逐渐递增。
```

字段含义：

```text
seq：
    lcdProducerTask 生成 LCD 显示消息的序号。
    每生成一条 AppLcdMessage_t，sequence 自增一次。

tick：
    生成这条显示消息时的系统 tick。

count：
    串口打印那一瞬间，lcdQueue 中还没被消费的消息数量。

space：
    lcdQueue 剩余可用空间。
    当前队列长度是 8，所以 space=8 表示队列为空。

drops：
    lcdProducerTask 调用 osMessageQueuePut() 失败的次数。
    drops=0 表示没有丢消息。
```

现象分析：

```text
第一条日志 count=1 space=7，说明串口打印时队列里刚好有 1 条消息还没被 lcdTask 取走。
后续日志基本是 count=0 space=8，说明 lcdTask 消费消息很及时，队列没有积压。
drops 始终为 0，说明 producerTask 发送消息没有失败。
```

本次实验结论：

```text
lcdProducerTask 每 1000ms 生成一条 AppLcdMessage_t。
osMessageQueuePut() 把消息副本放入 lcdQueue。
lcdTask 阻塞等待 lcdQueue。
队列收到消息后，lcdTask 被唤醒并刷新 TFTLCD。
当前生产速度和消费速度匹配，没有队列积压，也没有消息丢失。
```

### 队列后续需要掌握的注意事项

#### 1. 队列满时的处理

当前发送函数使用：

```c
osMessageQueuePut(lcdQueueHandle, &message, 0U, 0U);
```

最后一个参数是 `0U`，表示：

```text
如果队列满了，不等待，立即返回失败。
```

所以程序里用 `lcdProducerDropCount` 统计发送失败次数：

```text
drops=0：没有丢消息。
drops 增加：生产者发送太快，队列已经满，消息被丢弃。
```

#### 2. timeout 参数

`osMessageQueuePut()` 和 `osMessageQueueGet()` 都有 timeout 参数。

常见写法：

```c
osMessageQueuePut(queue, &msg, 0, 0);
```

含义：

```text
不等待，队列满就立即失败。
```

```c
osMessageQueuePut(queue, &msg, 0, 100);
```

含义：

```text
队列满时最多等待 100 tick。
```

```c
osMessageQueueGet(queue, &msg, NULL, osWaitForever);
```

含义：

```text
队列为空时一直阻塞等待消息。
```

核心理解：

```text
timeout 决定任务是立即失败返回，还是进入 Blocked 等待。
```

#### 3. 队列传值，不要随便传危险指针

当前消息结构中使用：

```c
char text[48];
```

这是安全的，因为队列发送时复制的是整个 `AppLcdMessage_t`，包括 `text` 数组内容。

不推荐这样设计：

```c
typedef struct
{
    const char *text;
} AppLcdMessage_t;
```

原因：

```text
如果 text 指向发送任务里的局部数组，发送任务下一次循环修改它，
或者局部变量生命周期结束，lcdTask 读到的数据就可能异常。
```

#### 4. 多生产者，单消费者

当前模型是：

```text
lcdProducerTask -> lcdQueue -> lcdTask
```

后续可以扩展为：

```text
sensorTask      -> lcdQueue
keyTask         -> lcdQueue
uartCommandTask -> lcdQueue
timerTask       -> lcdQueue
                     |
                     v
                  lcdTask
```

这类模型很适合 TFTLCD：

```text
多个任务都可以请求显示内容。
但只有 lcdTask 真正操作 TFTLCD。
```

#### 5. 消息类型 type 的使用

当前消息定义中已经预留：

```c
APP_LCD_MSG_STATUS
APP_LCD_MSG_TICK
APP_LCD_MSG_LED
APP_LCD_MSG_ERROR
```

后续可以在 `lcdTask` 里根据 `type` 分发：

```c
switch (message->type)
{
case APP_LCD_MSG_TICK:
    // 显示 tick
    break;
case APP_LCD_MSG_LED:
    // 显示 LED 状态
    break;
case APP_LCD_MSG_ERROR:
    // 显示错误信息
    break;
default:
    break;
}
```

这样队列传递的就不是单纯字符串，而是一个明确的“事件消息”。

#### 6. 队列长度如何选择

队列不是越长越好。

```text
队列太短：容易满，drops 增加。
队列太长：占用更多 FreeRTOS heap。
```

队列内存占用大致和下面两个因素有关：

```text
队列长度 * 每条消息大小
```

例如：

```c
osMessageQueueNew(8, sizeof(AppLcdMessage_t), NULL);
```

表示队列内部至少要能缓存 8 个 `AppLcdMessage_t`。

后续调试内存时，需要结合：

```text
configTOTAL_HEAP_SIZE
xPortGetFreeHeapSize()
```

观察 FreeRTOS heap 是否足够。

### 队列后续实验建议

#### 实验 A：队列满实验

实验目标：

```text
理解生产速度大于消费速度时，队列会逐渐填满。
观察 count、space、drops 的变化。
```

实验设计：

```text
lcdProducerTask 每 10ms 发送一条 LCD 消息。
lcdTask 每收到一条消息后，故意延时 500ms 再继续接收下一条。
lcdQueue 长度保持为 8。
uartTask 每 1000ms 打印 count、space、drops。
```

预期现象：

```text
count 会逐渐接近 8。
space 会逐渐接近 0。
drops 会开始增加。
LCD 上的 seq 会跳号，因为部分消息发送失败被丢弃。
```

实验结论应该验证：

```text
如果生产者发送速度远快于消费者处理速度，队列只能暂存有限消息。
当队列满且 timeout = 0 时，新消息会发送失败。
```

#### 实验 B：多消息类型实验

实验目标：

```text
理解队列不只是传字符串，还可以传递带 type 的结构体事件。
```

实验设计：

```text
producerTask 轮流发送 TICK、LED、ERROR 三种消息。
lcdTask 根据 message.type 把不同消息显示到不同区域。
```

预期现象：

```text
TICK 显示在 tick 区域。
LED 状态显示在 LED 区域。
ERROR 或模拟错误显示在错误区域。
```

实验结论应该验证：

```text
队列可以作为任务间的事件通道。
消息结构体中的 type 字段可以帮助消费者区分不同业务。
```
