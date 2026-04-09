/*
 * FreeRTOS 任务 CPU 占用统计（依赖 SysInfoTest.c 的 TIM6 50us 时基 + vTaskGetRunTimeStats）
 * 设为 0 可关闭：不再启动 TIM6 统计、不创建 CpuStat 任务。
 */
#ifndef RTOS_CPU_STATS_H
#define RTOS_CPU_STATS_H

#ifndef RTOS_CPU_STATS_ENABLE
#define RTOS_CPU_STATS_ENABLE  1
#endif

#if RTOS_CPU_STATS_ENABLE

/* 统计与 vTaskList 串口打印间隔(ms)。调试卡死时可改小(如 200)；嫌串口吵或量产可改大(如 1000)。
 * 可在工程预处理器里 #define RTOS_CPU_STATS_PRINT_PERIOD_MS 覆盖。 */
#ifndef RTOS_CPU_STATS_PRINT_PERIOD_MS
#define RTOS_CPU_STATS_PRINT_PERIOD_MS  200U
#endif

/* 须在 vTaskStartScheduler() 之前调用（启动 TIM6 累加 ulHighFrequencyTimerTicks） */
void RtosCpuStats_TimerInit(void);

/* 须在其它应用任务 xTaskCreate 之后、vTaskStartScheduler() 之前调用 */
void RtosCpuStats_TaskCreate(void);

#endif /* RTOS_CPU_STATS_ENABLE */

#endif /* RTOS_CPU_STATS_H */
