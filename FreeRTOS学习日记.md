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
