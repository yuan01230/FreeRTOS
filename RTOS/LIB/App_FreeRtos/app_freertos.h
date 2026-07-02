#ifndef __APP_FREERTOS_H
#define __APP_FREERTOS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建应用层 FreeRTOS 任务。
 *
 * 该函数需要在 osKernelInitialize() 之后、osKernelStart() 之前调用。
 *
 * @param 无。
 * @retval 无。
 */
void App_FreeRTOS_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_FREERTOS_H */
