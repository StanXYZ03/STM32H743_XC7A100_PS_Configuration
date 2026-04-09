/*
 * File: ewWinDefaultTask.c
 */

#include "ewWinDefaultTask.h"
#include "bsp_lcd_rgb.h"
#include "dma2d_wait.h"
#include "bsp_dwt.h"

#define UI_VRAM_LOGICAL_ADDR    0xC0000000U
#define UI_VRAM_PHYSICAL_0_ADDR 0xC0200000U
#define UI_VRAM_PHYSICAL_1_ADDR 0xC0300000U
#define UI_LCD_BRINGUP_TEST     0U

extern volatile uint8_t g_sdram_ready;
void MainTask(void);

static void UI_DisableUnalignedFaultTrap(void)
{
    SCB->CCR &= ~(uint32_t)SCB_CCR_UNALIGN_TRP_Msk;
    __DSB();
    __ISB();
}

static void UI_Framebuffer_Init(void)
{
    uint32_t i;
    uint16_t *p_logic = (uint16_t *)UI_VRAM_LOGICAL_ADDR;
    uint16_t *p0 = (uint16_t *)UI_VRAM_PHYSICAL_0_ADDR;
    uint16_t *p1 = (uint16_t *)UI_VRAM_PHYSICAL_1_ADDR;
    const uint32_t logic_pixels = 2U * 800U * 480U;
    const uint32_t phys_pixels  = 480U * 800U;

    for (i = 0U; i < logic_pixels; i++)
    {
        p_logic[i] = 0U;
    }

    for (i = 0U; i < phys_pixels; i++)
    {
        p0[i] = 0x0000U;
        p1[i] = 0x0000U;
    }

    SCB_CleanDCache_by_Addr((uint32_t *)UI_VRAM_LOGICAL_ADDR, logic_pixels * 2U);
    SCB_CleanDCache_by_Addr((uint32_t *)UI_VRAM_PHYSICAL_0_ADDR, phys_pixels * 2U);
    SCB_CleanDCache_by_Addr((uint32_t *)UI_VRAM_PHYSICAL_1_ADDR, phys_pixels * 2U);
}

void ewWinDefaultTask(void const * argument)
{
    (void)argument;

    while (g_sdram_ready == 0U)
    {
        osDelay(10);
    }

    UI_Framebuffer_Init();
		bsp_InitDWT();
    LCD_RGB_Init();

    if (LTDC_Layer1->CFBAR != UI_VRAM_PHYSICAL_0_ADDR)
    {
        LTDC_Layer1->CFBAR = UI_VRAM_PHYSICAL_0_ADDR;
        LTDC->SRCR = LTDC_SRCR_IMR;
    }

    LCD_RGB_BacklightOn();
#if (UI_LCD_BRINGUP_TEST == 1U)
    LCD_RGB_Fill(0xF800U);
    osDelay(1000);
    LCD_RGB_Fill(0x07E0U);
    osDelay(1000);
    LCD_RGB_Fill(0x001FU);
    osDelay(1000);
    LCD_RGB_Fill(0xFFFFU);
    osDelay(2000);
    LCD_RGB_Fill(0x0000U);
#elif (UI_LCD_BRINGUP_TEST == 2U)
    LCD_RGB_Fill(0xFFFFU);
    for (;;) { osDelay(1000); }
#endif
    __HAL_RCC_DMA2D_CLK_ENABLE();
    DMA2D_Wait_Init();
    UI_DisableUnalignedFaultTrap();

    MainTask();

    for (;;)
    {
        osDelay(1000);
    }
}

