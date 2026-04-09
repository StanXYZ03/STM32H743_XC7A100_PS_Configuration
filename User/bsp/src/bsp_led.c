/*
*********************************************************************************************************
*
*    Module: LED driver
*    Notes : Adapted for BIDT_GPEP core board.
*            LED1 -> PA0
*            LED2 -> PB4
*            LED3 -> PB9
*            LED4 -> PB15
*
*********************************************************************************************************
*/

#include "bsp.h"

#define LED_ACTIVE_LEVEL    GPIO_PIN_SET
#define LED_INACTIVE_LEVEL  GPIO_PIN_RESET

static uint8_t LedToPortPin(uint8_t _no, GPIO_TypeDef **_port, uint16_t *_pin)
{
    switch (_no)
    {
    case 1:
        *_port = GPIOA;
        *_pin = GPIO_PIN_0;
        return 1;

    case 2:
        *_port = GPIOB;
        *_pin = GPIO_PIN_4;
        return 1;

    case 3:
        *_port = GPIOB;
        *_pin = GPIO_PIN_9;
        return 1;

    case 4:
        *_port = GPIOB;
        *_pin = GPIO_PIN_15;
        return 1;

    default:
        return 0;
    }
}

void bsp_InitLed(void)
{
    GPIO_InitTypeDef gpio_init;

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

    gpio_init.Pin = GPIO_PIN_0;
    HAL_GPIO_Init(GPIOA, &gpio_init);

    gpio_init.Pin = GPIO_PIN_4 | GPIO_PIN_9 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOB, &gpio_init);

    bsp_LedOff(1);
    bsp_LedOff(2);
    bsp_LedOff(3);
    bsp_LedOff(4);
}

void bsp_LedOn(uint8_t _no)
{
    GPIO_TypeDef *port;
    uint16_t pin;

    if (LedToPortPin(_no, &port, &pin) == 0)
    {
        return;
    }

    HAL_GPIO_WritePin(port, pin, LED_ACTIVE_LEVEL);
}

void bsp_LedOff(uint8_t _no)
{
    GPIO_TypeDef *port;
    uint16_t pin;

    if (LedToPortPin(_no, &port, &pin) == 0)
    {
        return;
    }

    HAL_GPIO_WritePin(port, pin, LED_INACTIVE_LEVEL);
}

void bsp_LedToggle(uint8_t _no)
{
    GPIO_TypeDef *port;
    uint16_t pin;

    if (LedToPortPin(_no, &port, &pin) == 0)
    {
        return;
    }

    HAL_GPIO_TogglePin(port, pin);
}

uint8_t bsp_IsLedOn(uint8_t _no)
{
    GPIO_TypeDef *port;
    uint16_t pin;

    if (LedToPortPin(_no, &port, &pin) == 0)
    {
        return 0;
    }

    return (HAL_GPIO_ReadPin(port, pin) == LED_ACTIVE_LEVEL) ? 1 : 0;
}

/***************************** END OF FILE *********************************/

