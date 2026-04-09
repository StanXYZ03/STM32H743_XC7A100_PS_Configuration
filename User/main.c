#include "includes.h"
#include "dma2d_wait.h"
#include "bsp_fmc_sdram.h"
#include "gui_heap_experiment.h"
#include "bsp_lcd_rgb.h"
#include "ui/MainTask.h"
#include "ui/UI_WaveformCtrl.h"
#include "rtos_cpu_stats.h"
#include <stdbool.h>
#include <math.h>
#include <stdio.h>

/*
 * 显存地址定义（必须与 LCDConf_Lin_Template.c 中的宏完全一致）：
 *
 *   0xC0000000 ~ 0xC00BB7FF  逻辑横屏子帧 0（800×480×2 = 768 KB）
 *   0xC00BB800 ~ 0xC0176FFF  逻辑横屏子帧 1（800×480×2 = 768 KB）
 *   0xC0200000 ~ 0xC02BB7FF  物理竖屏缓冲 0（480×800×2 = 768 KB）← LTDC Ping
 *   0xC0300000 ~ 0xC03BB7FF  物理竖屏缓冲 1（480×800×2 = 768 KB）← LTDC Pong
 *
 * 逻辑子帧 1 最高地址 0xC0176FFF < 物理缓冲 0 起始地址 0xC0200000，无重叠。
 */
#define VRAM_LOGICAL_ADDR       0xC0000000U
#define VRAM_PHYSICAL_0_ADDR    0xC0200000U
#define VRAM_PHYSICAL_1_ADDR    0xC0300000U

/*
 * 硬件诊断模式：直接测试 LTDC 输出，绕过 emWin 和旋转
 * 用于隔离问题是 LTDC/面板硬件问题还是软件问题
 */
#define LCD_DIAGNOSTIC_MODE     0

/*
 * 绕过旋转测试：emWin 直接使用物理缓冲区（竖屏模式）
 * 用于确认问题是否在旋转算法层面
 * 0 = 正常模式（横屏逻辑缓冲区 + 旋转到竖屏）
 * 1 = 竖屏直接模式（跳过旋转，调试用）
 */
#define LCD_BYPASS_ROTATION     0

/*
 * DMA2D 阶段一测试：在 AXI SRAM 上验证 DMA2D R2M 硬件是否正常
 * 1 = 运行测试，失败则 Error_Handler，通过则继续
 * 0 = 跳过测试
 */
/* DMA2D 初始化已移植到 dma2d_wave.c，此处仅保留 Phase1 可选自检（GUI 前） */
#define DMA2D_PHASE1_TEST       0   /* 1=运行测试 0=跳过 */
#define DMA2D_PHASE2_TEST       0   /* 已废弃：DMA2D 初始化在 dma2d_wave，不再显示红块 */

#if (DMA2D_PHASE1_TEST == 1)
/**
 * DMA2D 阶段一：隔离测试，使用 AXI SRAM（非 SDRAM）
 * 验证 DMA2D R2M 填色能否正确写入内存并读回。
 * 通过则返回 true，失败则 Error_Handler。
 */
static bool DMA2D_Phase1_Test(void)
{
#define PHASE1_W        64
#define PHASE1_H        64
#define PHASE1_PIXELS   (PHASE1_W * PHASE1_H)
#define PHASE1_BYTES    (PHASE1_PIXELS * 2U)
#define PHASE1_COLOR    0xF800U   /* RGB565 红色 */

    /* AXI SRAM 末尾 8KB（0x2407E000），避免覆盖 0x24000000 处的 BSS/堆等关键数据 */
    volatile uint16_t *buf = (volatile uint16_t *)0x2407E000U;
    uint32_t i;

    __HAL_RCC_DMA2D_CLK_ENABLE();

    /* 先清零，避免读到未初始化数据 */
    for (i = 0U; i < PHASE1_PIXELS; i++) {
        buf[i] = 0U;
    }

    /* DMA2D R2M 填色 */
    while (DMA2D->CR & DMA2D_CR_START) {}
    DMA2D->CR     = 0x00030000UL;                    /* R2M 模式 */
    DMA2D->OCOLR  = (uint32_t)PHASE1_COLOR;
    DMA2D->OMAR   = (uint32_t)buf;
    DMA2D->OOR    = 0U;
    DMA2D->OPFCCR = 0x02U;                            /* RGB565 */
    DMA2D->NLR    = (PHASE1_W << 16U) | PHASE1_H;
    DMA2D->CR    |= DMA2D_CR_START;
    while (DMA2D->CR & DMA2D_CR_START) {}

    /* AXI SRAM 为 Write-Back Cache，DMA2D 直写内存，CPU 读前需 Invalidate */
    SCB_InvalidateDCache_by_Addr((uint32_t *)buf, PHASE1_BYTES);

    for (i = 0U; i < PHASE1_PIXELS; i++) {
        if (buf[i] != PHASE1_COLOR) {
            return false;
        }
    }
    return true;
}
#endif

static void SDRAM_QuickCheck(void)
{
    volatile uint32_t *p;
    uint32_t old0;
    uint32_t old1;

    p = (volatile uint32_t *)EXT_SDRAM_ADDR;
    old0 = p[0];
    old1 = p[1];

    p[0] = 0x55AA55AAU;
    p[1] = 0xAA55AA55U;

    if ((p[0] != 0x55AA55AAU) || (p[1] != 0xAA55AA55U))
    {
        Error_Handler(__FILE__, __LINE__);
    }

    p[0] = old0;
    p[1] = old1;
}

/* 初始化所有帧缓冲区，防止显示随机数据 */
static void Framebuffer_Init(void)
{
    uint32_t i;
    uint16_t *p_logic = (uint16_t *)VRAM_LOGICAL_ADDR;
    uint16_t *p0 = (uint16_t *)VRAM_PHYSICAL_0_ADDR;
    uint16_t *p1 = (uint16_t *)VRAM_PHYSICAL_1_ADDR;
    /*
     * NUM_BUFFERS=2：emWin 使用两个逻辑横屏子帧，分别起始于：
     *   子帧 0：VRAM_LOGICAL_ADDR + 0           = 0xC0000000
     *   子帧 1：VRAM_LOGICAL_ADDR + 800*480*2   = 0xC00BB800
     * 两个子帧都必须清零，否则 emWin 首次切换到子帧 1 时会显示 SDRAM 随机数据。
     */
    const uint32_t logic_pixels = 2U * 800U * 480U;  /* 覆盖两个逻辑子帧 */
    const uint32_t phys_pixels  = 480U * 800U;

    /* 清零全部逻辑缓冲区（包含两个 emWin 子帧） */
    for (i = 0U; i < logic_pixels; i++)
    {
        p_logic[i] = 0U;
    }

    /* 初始化物理缓冲区为黑色（背光开启前 LTDC 显示此内容） */
    for (i = 0U; i < phys_pixels; i++)
    {
        p0[i] = 0x0000U;
        p1[i] = 0x0000U;
    }

    /* Clean D-Cache：确保上述写入从 L1 Cache 推入 SDRAM */
    SCB_CleanDCache_by_Addr((uint32_t *)VRAM_LOGICAL_ADDR, logic_pixels * 2U);
    SCB_CleanDCache_by_Addr((uint32_t *)VRAM_PHYSICAL_0_ADDR, phys_pixels * 2U);
    SCB_CleanDCache_by_Addr((uint32_t *)VRAM_PHYSICAL_1_ADDR, phys_pixels * 2U);
}

#if (LCD_DIAGNOSTIC_MODE == 1)
/*
 * 硬件诊断测试：直接填充显存测试 LTDC 输出
 * 绕过 emWin 和旋转，用于确认是硬件问题还是软件问题
 */
static void LCD_DiagnosticTest(void)
{
    uint32_t i;
    uint16_t *p = (uint16_t *)VRAM_PHYSICAL_0_ADDR;
    const uint32_t pixels = 480U * 800U;

    /* 测试1: 纯红色 */
    for (i = 0U; i < pixels; i++) { p[i] = 0xF800U; }
    SCB_CleanDCache_by_Addr((uint32_t *)VRAM_PHYSICAL_0_ADDR, pixels * 2U);
    HAL_Delay(1000U);

    /* 测试2: 纯绿色 */
    for (i = 0U; i < pixels; i++) { p[i] = 0x07E0U; }
    SCB_CleanDCache_by_Addr((uint32_t *)VRAM_PHYSICAL_0_ADDR, pixels * 2U);
    HAL_Delay(1000U);

    /* 测试3: 纯蓝色 */
    for (i = 0U; i < pixels; i++) { p[i] = 0x001FU; }
    SCB_CleanDCache_by_Addr((uint32_t *)VRAM_PHYSICAL_0_ADDR, pixels * 2U);
    HAL_Delay(1000U);

    /* 测试4: 白色 */
    for (i = 0U; i < pixels; i++) { p[i] = 0xFFFFU; }
    SCB_CleanDCache_by_Addr((uint32_t *)VRAM_PHYSICAL_0_ADDR, pixels * 2U);
    HAL_Delay(1000U);

    /* 测试5: 水平渐变条纹 (检测行同步) */
    for (i = 0U; i < pixels; i++) {
        uint16_t row = (uint16_t)(i / 480U);
        if (row < 200U) {
            p[i] = 0xF800U;  /* 红色 */
        } else if (row < 400U) {
            p[i] = 0x07E0U;  /* 绿色 */
        } else if (row < 600U) {
            p[i] = 0x001FU;  /* 蓝色 */
        } else {
            p[i] = 0xFFFFU;  /* 白色 */
        }
    }
    SCB_CleanDCache_by_Addr((uint32_t *)VRAM_PHYSICAL_0_ADDR, pixels * 2U);
    HAL_Delay(2000U);

    /* 测试6: 垂直渐变条纹 (检测列同步) */
    for (i = 0U; i < pixels; i++) {
        uint16_t col = (uint16_t)(i % 480U);
        if (col < 120U) {
            p[i] = 0xF800U;  /* 红色 */
        } else if (col < 240U) {
            p[i] = 0x07E0U;  /* 绿色 */
        } else if (col < 360U) {
            p[i] = 0x001FU;  /* 蓝色 */
        } else {
            p[i] = 0xFFFFU;  /* 白色 */
        }
    }
    SCB_CleanDCache_by_Addr((uint32_t *)VRAM_PHYSICAL_0_ADDR, pixels * 2U);
    /* 此测试无限循环，观察屏幕 */
    while (1) { HAL_Delay(1000U); }
}
#endif

/*
 * 假数据生成任务 —— 用于在没有真实 ADC 接入时验证波形显示管线
 *
 * CH1：3 个周期的正弦波，幅度 ±1500 ADC 计数（占量程 73%）
 * CH2：2 个周期的余弦波，幅度 ±900  ADC 计数（占量程 44%），略有相位漂移
 * 发送速率：50 fps（每 20 ms 一帧），相位每帧递进产生"滚动"效果
 *
 * ADC 原始值范围：0 ~ 4095，中点 2048
 * 若幅度溢出范围，自动截断到 [0, 4095]
 */
static void ADC_FakeData_Task(void *arg)
{
    static uint16_t ch1[ADC_FRAME_SIZE];
    static uint16_t ch2[ADC_FRAME_SIZE];
    float  phase = 0.0f;
    uint32_t i;

    (void)arg;

    for (;;)
    {
        if (!UI_Waveform_IsStreamActive()) {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        for (i = 0; i < ADC_FRAME_SIZE; i++)
        {
            float t   = (float)i / (float)ADC_FRAME_SIZE;
            int   v1  = 2048 + (int)(1500.0f * sinf(2.0f * 3.14159265f * 3.0f * t + phase));
            int   v2  = 2048 + (int)( 900.0f * cosf(2.0f * 3.14159265f * 2.0f * t + phase * 0.6f));

            /* 截断到合法 ADC 范围 */
            if (v1 < 0)    v1 = 0;
            if (v1 > 4095) v1 = 4095;
            if (v2 < 0)    v2 = 0;
            if (v2 > 4095) v2 = 4095;

            ch1[i] = (uint16_t)v1;
            ch2[i] = (uint16_t)v2;
        }

        UI_Waveform_SendCH1(ch1, ADC_FRAME_SIZE);
        UI_Waveform_SendCH2(ch2, ADC_FRAME_SIZE);

        /* 仅 Roll 模式递进相位，产生滚动效果；Y-T 模式保持 phase 不变，波形静止 */
        if (UI_Waveform_GetDisplayMode() == UI_WAVEFORM_MODE_ROLL) {
            phase += 0.05f;
            if (phase >= 2.0f * 3.14159265f)
                phase -= 2.0f * 3.14159265f;
        }

        vTaskDelay(pdMS_TO_TICKS(20));  /* 50 fps */
    }
}

int main(void)
{
    System_Init();

    HAL_SuspendTick();
    __set_PRIMASK(1);

    bsp_Init();
    printf("\r\n[BOOT] OK\r\n");

#if (DMA2D_PHASE1_TEST == 1)
    if (!DMA2D_Phase1_Test()) {
        Error_Handler(__FILE__, __LINE__);
    }
#endif

    bsp_InitExtSDRAM();
    SDRAM_QuickCheck();
    printf("[BOOT] SDRAM OK\r\n");

#if (SDRAM_GUI_HEAP_TEST_AT_BOOT != 0)
    {
        uint32_t e = bsp_TestExtSDRAM_Block((uint32_t)SDRAM_APP_BUF, GUI_CONF_EXPERIMENT_NUMBYTES);
        printf("[BOOT] SDRAM GUI heap region test err=%lu (0=ok, size=%lu)\r\n",
               (unsigned long)e, (unsigned long)GUI_CONF_EXPERIMENT_NUMBYTES);
        if (e != 0U) {
            Error_Handler(__FILE__, __LINE__);
        }
    }
#endif

    /* 初始化帧缓冲区，防止花屏 */
    Framebuffer_Init();

    LCD_RGB_Init();
    printf("[BOOT] LTDC FB=0x%08lX\r\n", (unsigned long)LCD_RGB_FB_ADDR);

    /* 试验：PG10 拉高 */
    {
        GPIO_InitTypeDef gpio_pg10 = {0};

        __HAL_RCC_GPIOG_CLK_ENABLE();
        gpio_pg10.Pin   = GPIO_PIN_10;
        gpio_pg10.Mode  = GPIO_MODE_OUTPUT_PP;
        gpio_pg10.Pull  = GPIO_NOPULL;
        gpio_pg10.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(GPIOG, &gpio_pg10);
        HAL_GPIO_WritePin(GPIOG, GPIO_PIN_10, GPIO_PIN_SET);
    }

    /* 与 LCD_RGB_FB_ADDR / 物理缓冲 0 一致 */
    if (LTDC_Layer1->CFBAR != VRAM_PHYSICAL_0_ADDR) {
        LTDC_Layer1->CFBAR = VRAM_PHYSICAL_0_ADDR;
        LTDC->SRCR = LTDC_SRCR_IMR;
    }

    LCD_RGB_BacklightOn();

    /* DMA2D 时钟使能：须在 MainTask 首次 GUI 绘制前完成（原 Phase2 测试在此处执行）
     * 否则 GUI_FillRect、DrawGridFull 等使用 DMA2D 时时钟未开，导致背景显示不全 */
    __HAL_RCC_DMA2D_CLK_ENABLE();
    DMA2D_Wait_Init();
    /* emWin 启动前再清一次 UNALIGN_TRP（若此前固件未烧录或某路径曾改写 CCR） */
    Bsp_DisableUnalignedFaultTrap();

#if (LCD_DIAGNOSTIC_MODE == 1)
    /* 硬件诊断测试：绕过 emWin 直接测试 LTDC */
    LCD_DiagnosticTest();
#else
    if (xTaskCreate((TaskFunction_t)MainTask, "emWin", 8192, NULL, 2, NULL) != pdPASS) {
        printf("[BOOT] FAIL emWin\r\n");
        Error_Handler(__FILE__, __LINE__);
    }

    /* 假 ADC 数据 */
    if (xTaskCreate(ADC_FakeData_Task, "FakeADC", 2048, NULL, 1, NULL) != pdPASS) {
        printf("[BOOT] FAIL FakeADC\r\n");
        Error_Handler(__FILE__, __LINE__);
    }

#if RTOS_CPU_STATS_ENABLE
    /* TIM6 50us 时基，供 FreeRTOS vTaskGetRunTimeStats（须在调度器启动前） */
    RtosCpuStats_TimerInit();
    RtosCpuStats_TaskCreate();
#endif

    /* 启动调度器前恢复系统滴答 */
    HAL_ResumeTick();
    __set_PRIMASK(0);

    printf("[BOOT] scheduler\r\n");
    vTaskStartScheduler();
#endif

    while (1)
    {
    }
}

#if ( configCHECK_FOR_STACK_OVERFLOW > 0 )
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    printf("\r\n[RTOS] STACK OVERFLOW: %s\r\n", (pcTaskName != NULL) ? pcTaskName : "?");
    for (;;) {}
}
#endif

#if ( configUSE_MALLOC_FAILED_HOOK == 1 )
void vApplicationMallocFailedHook(void)
{
    printf("\r\n[RTOS] pvPortMalloc failed (heap exhausted)\r\n");
    taskDISABLE_INTERRUPTS();
    for (;;) {}
}
#endif
