/**
  ******************************************************************************
  * @file    gpio.h
  * @brief   GPIO driver header for STM32H743
  *          Includes LED and other GPIO definitions
  ******************************************************************************
  */

#ifndef __GPIO_H
#define __GPIO_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

/* Key indicator LEDs are driven through MCP23017 GPA0..7. */
#define KEY_LED_COUNT          8U

/* Function Prototypes -------------------------------------------------------*/
void MX_GPIO_Init(void);
void KEY_LED_Init(void);
void KEY_LED_WriteMask(uint8_t mask);
void FMCU_PI_Init(void);
void FMCU_PI_Write(uint16_t value);

#ifdef __cplusplus
}
#endif

#endif /* __GPIO_H */
