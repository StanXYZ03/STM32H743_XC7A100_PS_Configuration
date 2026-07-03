/**
  ******************************************************************************
  * @file    bsp_ui_io_expander.c
  * @brief   MCP23017 software-I2C output driver for FMCU_PI[15:0] and key LEDs
  ******************************************************************************
  */

#include "bsp_ui_io_expander.h"

#define UI_I2C_GPIO         GPIOB
#define UI_I2C_SCL_PIN      GPIO_PIN_8
#define UI_I2C_SDA_PIN      GPIO_PIN_7

#define KEY_LED_MCP23017_ADDR7 0x26u
#define FMCU_PI_MCP23017_ADDR7 0x27u

#define MCP23017_REG_IODIRA 0x00u
#define MCP23017_REG_IODIRB 0x01u
#define MCP23017_REG_GPIOA  0x12u
#define MCP23017_REG_GPIOB  0x13u

static void ui_i2c_delay(void)
{
    volatile uint32_t i;

    for (i = 0U; i < 64U; ++i)
    {
        __NOP();
    }
}

static void ui_scl_write(GPIO_PinState state)
{
    UI_I2C_GPIO->BSRR = (state == GPIO_PIN_SET) ? UI_I2C_SCL_PIN : ((uint32_t)UI_I2C_SCL_PIN << 16U);
}

static void ui_sda_write(GPIO_PinState state)
{
    UI_I2C_GPIO->BSRR = (state == GPIO_PIN_SET) ? UI_I2C_SDA_PIN : ((uint32_t)UI_I2C_SDA_PIN << 16U);
}

static GPIO_PinState ui_sda_read(void)
{
    return ((UI_I2C_GPIO->IDR & UI_I2C_SDA_PIN) != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET;
}

static void ui_i2c_start(void)
{
    ui_sda_write(GPIO_PIN_SET);
    ui_scl_write(GPIO_PIN_SET);
    ui_i2c_delay();
    ui_sda_write(GPIO_PIN_RESET);
    ui_i2c_delay();
    ui_scl_write(GPIO_PIN_RESET);
    ui_i2c_delay();
}

static void ui_i2c_stop(void)
{
    ui_sda_write(GPIO_PIN_RESET);
    ui_i2c_delay();
    ui_scl_write(GPIO_PIN_SET);
    ui_i2c_delay();
    ui_sda_write(GPIO_PIN_SET);
    ui_i2c_delay();
}

static void ui_i2c_send_byte(uint8_t value)
{
    uint8_t bit;

    for (bit = 0U; bit < 8U; ++bit)
    {
        ui_sda_write(((value & 0x80U) != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        ui_i2c_delay();
        ui_scl_write(GPIO_PIN_SET);
        ui_i2c_delay();
        ui_scl_write(GPIO_PIN_RESET);
        ui_i2c_delay();
        value <<= 1;
    }

    ui_sda_write(GPIO_PIN_SET);
}

static uint8_t ui_i2c_wait_ack(void)
{
    uint8_t nack;

    ui_sda_write(GPIO_PIN_SET);
    ui_i2c_delay();
    ui_scl_write(GPIO_PIN_SET);
    ui_i2c_delay();
    nack = (ui_sda_read() == GPIO_PIN_SET) ? 1U : 0U;
    ui_scl_write(GPIO_PIN_RESET);
    ui_i2c_delay();
    return nack;
}

static uint8_t ui_i2c_write_reg(uint8_t addr7, uint8_t reg, uint8_t value)
{
    ui_i2c_start();
    ui_i2c_send_byte((uint8_t)(addr7 << 1));
    if (ui_i2c_wait_ack() != 0U)
    {
        ui_i2c_stop();
        return 1U;
    }

    ui_i2c_send_byte(reg);
    if (ui_i2c_wait_ack() != 0U)
    {
        ui_i2c_stop();
        return 1U;
    }

    ui_i2c_send_byte(value);
    if (ui_i2c_wait_ack() != 0U)
    {
        ui_i2c_stop();
        return 1U;
    }

    ui_i2c_stop();
    return 0U;
}

static void BSP_UI_IO_InitBus(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio_init.Pin = UI_I2C_SCL_PIN | UI_I2C_SDA_PIN;
    gpio_init.Mode = GPIO_MODE_OUTPUT_OD;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(UI_I2C_GPIO, &gpio_init);

    ui_i2c_stop();
}

void BSP_UI_IO_Init(void)
{
    BSP_UI_IO_InitBus();

    (void)ui_i2c_write_reg(FMCU_PI_MCP23017_ADDR7, MCP23017_REG_IODIRA, 0x00U);
    (void)ui_i2c_write_reg(FMCU_PI_MCP23017_ADDR7, MCP23017_REG_IODIRB, 0x00U);
}

void BSP_UI_IO_Write16(uint16_t value)
{
    (void)ui_i2c_write_reg(FMCU_PI_MCP23017_ADDR7, MCP23017_REG_GPIOA, (uint8_t)(value & 0x00FFU));
    (void)ui_i2c_write_reg(FMCU_PI_MCP23017_ADDR7, MCP23017_REG_GPIOB, (uint8_t)(value >> 8));
}

void BSP_UI_IO_InitKeyLeds(void)
{
    BSP_UI_IO_InitBus();
    (void)ui_i2c_write_reg(KEY_LED_MCP23017_ADDR7, MCP23017_REG_IODIRA, 0x00U);
    BSP_UI_IO_WriteKeyLeds(0x00U);
}

void BSP_UI_IO_WriteKeyLeds(uint8_t mask)
{
    (void)ui_i2c_write_reg(KEY_LED_MCP23017_ADDR7, MCP23017_REG_GPIOA, (uint8_t)~mask);
}
