#ifndef DMA2D_WAIT_H
#define DMA2D_WAIT_H

#include "FreeRTOS.h"

/*
 * portMAX_DELAY 时实际使用的超时（毫秒）。超时后会 ABORT 并清标志，避免 MainTask 永久阻塞。
 * 可在包含本头文件前 #define 覆盖。
 */
#ifndef DMA2D_WAIT_TIMEOUT_MS
#define DMA2D_WAIT_TIMEOUT_MS  500u
#endif

/* 须在使用 DMA2D 且调用 Wait 前执行一次（例如 main 里 RCC 开 DMA2D 后） */
void DMA2D_Wait_Init(void);

/* 在 DMA2D->CR |= DMA2D_CR_START 之后调用：TC/TE/CE 等中断内 give，本任务阻塞让出 CPU */
void DMA2D_Wait_TransferComplete(TickType_t xTicksToWait);

/* 因等待超时而执行硬件恢复的次数（调试用，永不递减） */
uint32_t DMA2D_Wait_GetTimeoutRecoverCount(void);

#endif /* DMA2D_WAIT_H */
