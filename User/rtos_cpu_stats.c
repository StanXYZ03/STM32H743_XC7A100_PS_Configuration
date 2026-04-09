/*
 * FreeRTOS 运行时间统计：串口周期性打印各任务 CPU%（printf → UART，见 bsp_uart_fifo fputc）
 */
#include "rtos_cpu_stats.h"

#if RTOS_CPU_STATS_ENABLE

#include "includes.h"
#include "ui/lcd_rotate_request.h"
#include <stdio.h>

#define CPU_STATS_BUF_BYTES  1024U

static char s_rtos_stats_buf[CPU_STATS_BUF_BYTES];

void RtosCpuStats_TimerInit(void)
{
    vSetupSysInfoTest();
}

static void RtosCpuStats_Task(void *arg)
{
    (void)arg;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(RTOS_CPU_STATS_PRINT_PERIOD_MS));
        printf("\r\n======== FreeRTOS Run-Time Stats (TIM6 50us base) ========\r\n");
        printf("g_lcdconf_vsync_timeout_cnt = %lu  (LTDC VSYNC 等待超时次数，底板噪声时可能增加)\r\n",
               (unsigned long)g_lcdconf_vsync_timeout_cnt);
        vTaskGetRunTimeStats(s_rtos_stats_buf);
        printf("%s", s_rtos_stats_buf);
        printf("-------- Task State List --------\r\n");
        vTaskList(s_rtos_stats_buf);
        printf("%s", s_rtos_stats_buf);
        printf("============================================================\r\n\r\n");
    }
}

void RtosCpuStats_TaskCreate(void)
{
    /* 优先级 1：与 FakeADC/Demo 同级，低于 emWin(2)；栈需容纳 sprintf 输出 */
    if (xTaskCreate(RtosCpuStats_Task, "CpuStat", 1024, NULL, 1, NULL) != pdPASS) {
        printf("RtosCpuStats_TaskCreate failed\r\n");
    }
}

#endif /* RTOS_CPU_STATS_ENABLE */
