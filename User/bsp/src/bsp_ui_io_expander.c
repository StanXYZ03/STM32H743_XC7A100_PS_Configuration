/*
 * MCP23017 @ I2C 地址 0x26（7-bit），PB8=SCL、PB7=SDA，GPIO 模拟 I2C（开漏 + 外部上拉）。
 */

#include "bsp.h"
#include "bsp_ui_io_expander.h"

#define UI_I2C_GPIO     GPIOB
#define UI_I2C_SCL_PIN  GPIO_PIN_8
#define UI_I2C_SDA_PIN  GPIO_PIN_7

#define UI_I2C_ADDR7    0x26u
#define UI_I2C_WR       0u
#define MCP23017_REG_GPIOB  0x13u

/* STM32H7 GPIO_TypeDef 为 BSRRL/BSRRH 分寄存器，与 bsp_i2c_gpio.c 一致 */
#define UI_SCL_1()     (UI_I2C_GPIO->BSRR = (uint32_t)UI_I2C_SCL_PIN)
#define UI_SCL_0()     (UI_I2C_GPIO->BSRR = ((uint32_t)UI_I2C_SCL_PIN << 16))
#define UI_SDA_1()     (UI_I2C_GPIO->BSRR = (uint32_t)UI_I2C_SDA_PIN)
#define UI_SDA_0()     (UI_I2C_GPIO->BSRR = ((uint32_t)UI_I2C_SDA_PIN << 16))

#define UI_SDA_READ()  (((UI_I2C_GPIO->IDR) & UI_I2C_SDA_PIN) != 0U)

static void ui_i2c_delay(void)
{
    bsp_DelayUS(2);
}

static void ui_i2c_stop(void)
{
    UI_SDA_0();
    ui_i2c_delay();
    UI_SCL_1();
    ui_i2c_delay();
    UI_SDA_1();
    ui_i2c_delay();
}

static void ui_i2c_start(void)
{
    UI_SDA_1();
    UI_SCL_1();
    ui_i2c_delay();
    UI_SDA_0();
    ui_i2c_delay();
    UI_SCL_0();
    ui_i2c_delay();
}

static void ui_i2c_send_byte(uint8_t b)
{
    uint8_t i;

    for (i = 0U; i < 8U; i++) {
        if ((b & 0x80U) != 0U) {
            UI_SDA_1();
        } else {
            UI_SDA_0();
        }
        ui_i2c_delay();
        UI_SCL_1();
        ui_i2c_delay();
        UI_SCL_0();
        if (i == 7U) {
            UI_SDA_1();
        }
        b <<= 1;
    }
}

static uint8_t ui_i2c_read_byte(void)
{
    uint8_t i;
    uint8_t v = 0U;

    for (i = 0U; i < 8U; i++) {
        v <<= 1U;
        UI_SCL_1();
        ui_i2c_delay();
        if (UI_SDA_READ()) {
            v++;
        }
        UI_SCL_0();
        ui_i2c_delay();
    }
    return v;
}

static uint8_t ui_i2c_wait_ack(void)
{
    uint8_t re;

    UI_SDA_1();
    ui_i2c_delay();
    UI_SCL_1();
    ui_i2c_delay();
    re = UI_SDA_READ() ? 1U : 0U;
    UI_SCL_0();
    ui_i2c_delay();
    return re;
}

static void ui_i2c_nack(void)
{
    UI_SDA_1();
    ui_i2c_delay();
    UI_SCL_1();
    ui_i2c_delay();
    UI_SCL_0();
    ui_i2c_delay();
}

void BSP_UI_IO_Init(void)
{
    GPIO_InitTypeDef gpio;

    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio.Mode      = GPIO_MODE_OUTPUT_OD;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_LOW;
    gpio.Pin       = UI_I2C_SCL_PIN | UI_I2C_SDA_PIN;
    gpio.Alternate = 0U;
    HAL_GPIO_Init(UI_I2C_GPIO, &gpio);

    ui_i2c_stop();

    /* IODIRA=0 输出(接 LED)，IODIRB=0xFF 输入（编码器/按键） */
    ui_i2c_start();
    ui_i2c_send_byte((uint8_t)((UI_I2C_ADDR7 << 1) | UI_I2C_WR));
    (void)ui_i2c_wait_ack();
    ui_i2c_send_byte(0x00u); /* IODIRA */
    (void)ui_i2c_wait_ack();
    ui_i2c_send_byte(0x00u);
    (void)ui_i2c_wait_ack();
    ui_i2c_send_byte(0xFFu);
    (void)ui_i2c_wait_ack();
    ui_i2c_stop();
}

int BSP_UI_IO_ReadPortB(uint8_t *out)
{
    if (out == NULL) {
        return -1;
    }

    ui_i2c_start();
    ui_i2c_send_byte((uint8_t)((UI_I2C_ADDR7 << 1) | UI_I2C_WR));
    if (ui_i2c_wait_ack() != 0U) {
        ui_i2c_stop();
        return -1;
    }
    ui_i2c_send_byte(MCP23017_REG_GPIOB);
    if (ui_i2c_wait_ack() != 0U) {
        ui_i2c_stop();
        return -1;
    }
    ui_i2c_start();
    ui_i2c_send_byte((uint8_t)((UI_I2C_ADDR7 << 1) | 1u)); /* read */
    if (ui_i2c_wait_ack() != 0U) {
        ui_i2c_stop();
        return -1;
    }
    *out = ui_i2c_read_byte();
    ui_i2c_nack();
    ui_i2c_stop();
    return 0;
}
