/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   GPIO driver for STM32H743
  *          LED control and extended GPIO for FPGA communication
  ******************************************************************************
  */

#include "gpio.h"
#include "bsp_ui_io_expander.h"

/**
  * @brief  Initialize GPIO
  * @param  None
  * @retval None
  */
void MX_GPIO_Init(void)
{
    /* Enable GPIO clocks */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOI_CLK_ENABLE();

    /* Initialize local key indicator LEDs */
    KEY_LED_Init();
    FMCU_PI_Init();
}

/**
  * @brief  Initialize STM32 local key indicator LEDs
  * @param  None
  * @retval None
  */
void KEY_LED_Init(void)
{
    BSP_UI_IO_InitKeyLeds();
}

void KEY_LED_WriteMask(uint8_t mask)
{
    BSP_UI_IO_WriteKeyLeds(mask);
}

void FMCU_PI_Init(void)
{
    BSP_UI_IO_Init();
    FMCU_PI_Write(0x8000U);
}

void FMCU_PI_Write(uint16_t value)
{
    BSP_UI_IO_Write16(value);
}
