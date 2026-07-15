# FreeRTOS 学习计划

本计划基于当前工程环境编写：

- 开发板/芯片：普中麒麟 F407，STM32F407ZGT6
- 工程路径：`F:\CLion\FreeRTOS_CUBEMX\RTOS`
- 生成工具：STM32CubeMX
- IDE：CLion
- 构建系统：CMake + Ninja + STM32CubeCLT
- RTOS 接口：CMSIS-RTOS v2
- HAL Timebase：TIM2
- 系统时钟：HSE 25 MHz，经 PLL 到 SYSCLK 168 MHz，PLLQ 7 得到 48 MHz
- 当前基础任务：`defaultTask`，栈大小 256 words

## 学习目标

学完后你应该能做到：

- 看懂 CubeMX 生成的 FreeRTOS 工程结构。
- 独立创建任务、设置优先级、控制任务延时。
- 使用队列、信号量、事件标志、互斥锁进行任务通信。
- 理解中断与 FreeRTOS API 的关系，正确使用 `FromISR` 类接口。
- 会检查任务栈、FreeRTOS 堆、栈溢出和 malloc 失败。
- 能完成一个小型多任务嵌入式项目。

## 阶段 0：固定工程基线

目标：确保工程配置稳定，后面所有实验都从同一个可靠基础出发。

需要确认：

- CubeMX 中 `SYS -> Timebase Source` 选择 `TIM2`。
- FreeRTOS Interface 选择 `CMSIS_V2`。
- `configCHECK_FOR_STACK_OVERFLOW = 2`。
- `configUSE_MALLOC_FAILED_HOOK = 1`。
- `configENABLE_FPU = 1`。
- `defaultTask` 栈大小为 `256 words`。
- CLion CMake 配置带有：

```text
-DCMAKE_TOOLCHAIN_FILE=F:/CLion/FreeRTOS_CUBEMX/RTOS/cmake/gcc-arm-none-eabi.cmake
```

验证方式：

```text
cmake --build --preset Debug -j 6
```

通过标准：能生成 `RTOS.elf`，没有编译错误。

## 阶段 1：任务基础

目标：理解 FreeRTOS 最核心的概念：任务、调度、延时、优先级。

建议实验：

1. 在 CubeMX 中新增两个任务：
   - `ledTask`
   - `uartTask`
2. `ledTask` 每 500 ms 翻转一次 LED。
3. `uartTask` 每 1000 ms 通过 USART1 打印一行文本。
4. 分别调整两个任务的优先级，观察运行效果。

重点理解：

- `osThreadNew()` 是如何创建任务的。
- `osDelay()` 会让出 CPU，不是裸机里的阻塞死等。
- 高优先级任务不能长期不释放 CPU。
- 同优先级任务会按时间片轮转，前提是开启时间片调度。

建议记录：

- LED 闪烁周期是否稳定。
- 串口打印是否稳定。
- 改变任务优先级后，现象有没有变化。

常见坑：

- 任务函数里不能写没有延时的死循环。
- 串口打印太频繁可能影响调度观察。
- 任务栈太小会导致异常或跑飞。

## 阶段 2：任务通信：队列

目标：掌握 FreeRTOS 中最常用、最安全的任务间传数据方式。

建议实验：

1. 创建一个按键扫描任务 `keyTask`。
2. 创建一个 LED 控制任务 `ledControlTask`。
3. `keyTask` 检测到按键事件后，把事件写入队列。
4. `ledControlTask` 从队列读取事件，并改变 LED 状态。

重点理解：

- 队列传递的是数据副本，不是直接共享变量。
- `osMessageQueuePut()` 用于发送消息。
- `osMessageQueueGet()` 用于接收消息。
- 队列可以让任务之间解耦。

建议记录：

- 队列长度设置为 1、5、10 时有什么区别。
- 接收任务阻塞等待队列时，CPU 是否还能运行其他任务。
- 队列满时发送任务会发生什么。

常见坑：

- 发送结构体时要确认队列元素大小正确。
- 不要把局部变量指针直接丢进队列后马上返回。
- 队列满时要处理返回值。

### 阶段 2 实验记录：TFTLCD 队列满实验

实验目标：

```text
观察生产者任务发送速度大于消费者任务处理速度时，队列如何被填满。
理解 count、space、drops 的实际含义。
```

实验配置：

```text
lcdProducerTask：每 10ms 发送一条 LCD 显示消息。
lcdTask：每消费一条消息后延时 500ms。
lcdQueue：长度为 8。
uartTask：每 1000ms 打印一次队列状态。
```

串口现象：

```text
[font] Init Flash... 0%
[flash] id=0x6817 sr=0x00
[font] Check Font... 0%
[font] Font Ready 100%

FreeRTOS queue TFTLCD demo start.
[queue] tick=601 count=1 space=7 drops=0
[queue] tick=1604 count=8 space=0 drops=91
[queue] tick=2607 count=8 space=0 drops=189
[queue] tick=3610 count=8 space=0 drops=288
[queue] tick=4613 count=8 space=0 drops=386
[queue] tick=5616 count=8 space=0 drops=484
[queue] tick=6619 count=8 space=0 drops=583
[queue] tick=7622 count=8 space=0 drops=681
[queue] tick=8625 count=8 space=0 drops=779
[queue] tick=9628 count=8 space=0 drops=877
[queue] tick=10631 count=8 space=0 drops=976
[queue] tick=11635 count=8 space=0 drops=1074
```

字段含义：

```text
count：当前队列中等待消费的消息数量。
space：当前队列剩余可写入空间。
drops：osMessageQueuePut() 发送失败次数。
```

现象分析：

```text
count=8 表示队列已经填满。
space=0 表示队列没有剩余空间。
drops 持续增加，表示 producerTask 继续发送消息，但队列已满，发送失败。
```

当前生产和消费速度：

```text
producerTask 每 10ms 生产一条消息，约每秒 100 条。
lcdTask 每 500ms 消费一条消息，约每秒 2 条。
```

因此队列满后，每秒理论丢失消息数量约为：

```text
100 - 2 = 98 条
```

实际串口数据中，`drops` 每秒大约增加 98 到 99，和理论一致。

实验结论：

```text
队列不是无限缓存。
队列只能缓冲短时间的生产/消费速度差。
当生产者长期快于消费者时，队列会被填满。
如果 osMessageQueuePut() 的 timeout = 0，队列满时会立即返回失败。
程序需要检查返回值，并根据业务决定丢弃、等待、覆盖旧消息或降低生产速度。
```

## 阶段 3：同步机制：信号量和事件标志

目标：理解“通知某件事发生了”和“传递数据”的区别。

建议实验 A：二值信号量

1. 创建 `buttonTask` 和 `workerTask`。
2. `buttonTask` 检测按键后释放信号量。
3. `workerTask` 等待信号量，拿到后执行一次动作。

建议实验 B：事件标志

1. 创建三个事件位：
   - `EVENT_KEY`
   - `EVENT_UART`
   - `EVENT_TIMER`
2. 不同任务设置不同事件位。
3. 一个 `eventTask` 等待一个或多个事件组合。

重点理解：

- 信号量适合表达“有一次事件发生”。
- 事件标志适合表达“多个条件组合”。
- 队列适合传数据，信号量适合同步，事件标志适合状态集合。

常见坑：

- 不要用信号量传复杂数据。
- 事件标志是否自动清除要配置清楚。
- 等待多个事件时，要区分“任意一个满足”和“全部满足”。

## 阶段 4：资源保护：互斥锁

目标：理解多个任务访问同一个资源时为什么需要保护。

建议实验：

1. 创建两个串口打印任务。
2. 两个任务都周期性打印字符串。
3. 先不加互斥锁，观察串口输出是否交叉混乱。
4. 再加互斥锁保护 `HAL_UART_Transmit()`。

重点理解：

- 互斥锁用于保护共享资源。
- 串口、I2C、SPI、全局数据结构都可能是共享资源。
- 互斥锁和二值信号量用途不同。
- 互斥锁有优先级继承机制，能缓解优先级反转。

常见坑：

- 拿到互斥锁后不要长时间延时。
- 不要忘记释放互斥锁。
- 不要在中断里使用普通互斥锁。

## 阶段 5：软件定时器

目标：学会使用 FreeRTOS 软件定时器处理周期性或延时性任务。

建议实验：

1. 创建一个周期性软件定时器，每 1000 ms 触发一次。
2. 在回调里设置事件标志。
3. 让任务收到事件后执行实际工作。

重点理解：

- 软件定时器回调运行在定时器服务任务中。
- 回调里不要做耗时操作。
- 软件定时器适合轻量级周期触发。
- 真正耗时的工作应该交给任务做。

常见坑：

- 软件定时器不是硬件定时器，精度受调度影响。
- 回调里不要阻塞等待。
- 定时器任务栈太小时也可能出问题。

## 阶段 6：中断与 FreeRTOS

目标：掌握中断中如何安全地通知任务。

建议实验：

1. 配置一个外部中断按键。
2. 在中断回调中释放信号量或设置事件标志。
3. 任务等待这个信号量或事件标志并处理按键动作。

重点理解：

- 中断里不能随便调用普通 FreeRTOS API。
- 中断里要使用 `FromISR` 类接口。
- 中断优先级数字必须大于等于 `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`，当前是 5。
- 数字越小，中断优先级越高。

优先级建议：

```text
FreeRTOS 可调用 API 的外设中断优先级：5 到 15
不要设成：0、1、2、3、4
SysTick / PendSV / TIM2 保持最低优先级 15
```

常见坑：

- 中断优先级配置太高，调用 RTOS API 后 HardFault。
- 在中断里做串口打印。
- 在中断里做复杂逻辑。

## 阶段 7：内存、栈和调试

目标：能定位 FreeRTOS 学习中最常见的崩溃原因。

建议实验：

1. 调用 `uxTaskGetStackHighWaterMark()` 查看任务剩余栈。
2. 故意把某个任务栈调小，观察栈溢出 hook。
3. 故意创建大量任务或队列，观察 malloc failed hook。
4. 通过串口打印各任务状态。

重点理解：

- 每个任务都有自己的栈。
- FreeRTOS 对象通常从 `configTOTAL_HEAP_SIZE` 中分配。
- `heap_4.c` 支持释放内存并合并空闲块。
- 栈溢出和堆不足是两类不同问题。

建议加入的 hook：

```c
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    taskDISABLE_INTERRUPTS();
    for (;;)
    {
    }
}

void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();
    for (;;)
    {
    }
}
```

注意：如果 CubeMX 已经生成相关声明或弱函数，应放在用户代码区中，避免重新生成时丢失。

## 阶段 8：综合项目

目标：把前面知识串起来，完成一个小型多任务系统。

推荐项目：多任务串口命令控制器

功能设计：

- `keyTask`：扫描按键，发送按键事件到队列。
- `ledTask`：根据事件控制 LED 模式。
- `uartRxTask`：接收串口命令。
- `cmdTask`：解析命令并发送控制消息。
- `statusTask`：每秒打印系统状态。
- `timer`：周期触发状态更新事件。

支持命令示例：

```text
led on
led off
led blink 500
status
heap
stack
```

应该练到的能力：

- 多任务拆分。
- 队列传递命令。
- 互斥锁保护串口。
- 软件定时器触发周期事件。
- 查看栈余量和堆余量。
- 中断或按键事件通知任务。

## 推荐学习节奏

如果每天学习 1 到 2 小时，可以按下面节奏：

| 周次 | 内容 | 目标 |
| --- | --- | --- |
| 第 1 周 | 工程基线、任务、延时、优先级 | 能独立创建多个任务 |
| 第 2 周 | 队列、信号量、事件标志 | 能完成任务间通信 |
| 第 3 周 | 互斥锁、软件定时器、中断 FromISR | 能处理共享资源和中断通知 |
| 第 4 周 | 内存/栈调试、综合项目 | 能完成一个小型 RTOS 应用 |

## 每次实验的记录模板

建议每做一个实验都记录下面几项：

```text
实验名称：
实验目标：
CubeMX 修改：
新增任务/队列/信号量：
关键代码位置：
观察到的现象：
遇到的问题：
解决方法：
还没理解的点：
```

这样以后回看时，你会清楚知道自己不是“看过”，而是真的“跑过、错过、修过”。

## 学习时的判断标准

不要只看代码能不能编译，要用下面几个问题检查自己是否真的掌握：

- 我能解释这个任务为什么要存在吗？
- 我能说明这个任务什么时候阻塞、什么时候运行吗？
- 我知道这段代码能不能放在中断里吗？
- 我知道这个 API 失败时返回什么吗？
- 我知道任务栈还剩多少吗？
- 我知道共享资源有没有被保护吗？

如果这些问题能回答清楚，FreeRTOS 的基础就很扎实了。

## 当前工程下一步建议

建议先从最小实验开始：

1. 在 CubeMX 中配置一个 LED GPIO 输出。
2. 新建 `ledTask`，每 500 ms 翻转 LED。
3. 新建 `uartTask`，每 1000 ms 打印一次系统运行时间。
4. 给串口打印加互斥锁。
5. 再加入按键和队列。

这条路线最稳，能把任务调度、延时、共享资源和任务通信自然串起来。
