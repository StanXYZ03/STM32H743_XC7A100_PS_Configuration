#include "ExpH743SpiBridge.h"

#include "spi.h"
#include "bsp_dwt.h"

#include <string.h>

#define EXPH743_CS_PORT              GPIOA
#define EXPH743_CS_PIN               GPIO_PIN_15
#define EXPH743_SPI_TIMEOUT_MS       20U
#define EXPH743_CS_SETUP_US          2U

static uint8_t s_spi_tx_buf[EXPH743_SPI_FRAME_SIZE];
static uint8_t s_spi_rx_buf[EXPH743_SPI_FRAME_SIZE];
static ExpH743SpiState_t s_state;

static void ExpH743Spi_ParseFrame(void)
{
    s_state.clk_sel = s_spi_rx_buf[0] & 0x1FU;
    s_state.mode = s_spi_rx_buf[1] & 0x0FU;

    s_state.payload_bytes[0] = s_spi_rx_buf[2];
    s_state.payload_bytes[1] = s_spi_rx_buf[3];
    s_state.payload_bytes[2] = s_spi_rx_buf[4];
    s_state.payload_bytes[3] = s_spi_rx_buf[5];

    s_state.payload_nibbles[0] = (uint8_t)(s_spi_rx_buf[2] >> 4);
    s_state.payload_nibbles[1] = (uint8_t)(s_spi_rx_buf[2] & 0x0FU);
    s_state.payload_nibbles[2] = (uint8_t)(s_spi_rx_buf[3] >> 4);
    s_state.payload_nibbles[3] = (uint8_t)(s_spi_rx_buf[3] & 0x0FU);
    s_state.payload_nibbles[4] = (uint8_t)(s_spi_rx_buf[4] >> 4);
    s_state.payload_nibbles[5] = (uint8_t)(s_spi_rx_buf[4] & 0x0FU);
    s_state.payload_nibbles[6] = (uint8_t)(s_spi_rx_buf[5] >> 4);
    s_state.payload_nibbles[7] = (uint8_t)(s_spi_rx_buf[5] & 0x0FU);

    s_state.rx_word0 = ((uint16_t)s_spi_rx_buf[0] << 8) | s_spi_rx_buf[1];
    s_state.rx_word1 = ((uint16_t)s_spi_rx_buf[2] << 8) | s_spi_rx_buf[3];
    s_state.rx_word2 = ((uint16_t)s_spi_rx_buf[4] << 8) | s_spi_rx_buf[5];
    s_state.xc7a_po = ((uint32_t)s_spi_rx_buf[6] << 24) |
                      ((uint32_t)s_spi_rx_buf[7] << 16) |
                      ((uint32_t)s_spi_rx_buf[8] << 8)  |
                      ((uint32_t)s_spi_rx_buf[9]);
    s_state.xc7a_pio = ((uint16_t)s_spi_rx_buf[10] << 8) |
                       ((uint16_t)s_spi_rx_buf[11]);
    s_state.fmcu_pi = ExpH743Spi_BuildFmcuPiValue(s_state.mode, s_state.payload_bytes);
}

void ExpH743Spi_Init(void)
{
    memset(&s_state, 0, sizeof(s_state));
    memset(s_spi_tx_buf, 0, sizeof(s_spi_tx_buf));
    memset(s_spi_rx_buf, 0, sizeof(s_spi_rx_buf));

    /* Same dummy MOSI pattern as the source project. */
    s_spi_tx_buf[0] = 0xA5U;
    s_spi_tx_buf[1] = 0x5AU;

    HAL_GPIO_WritePin(EXPH743_CS_PORT, EXPH743_CS_PIN, GPIO_PIN_SET);
}

HAL_StatusTypeDef ExpH743Spi_Transfer(void)
{
    HAL_StatusTypeDef status;

    HAL_GPIO_WritePin(EXPH743_CS_PORT, EXPH743_CS_PIN, GPIO_PIN_RESET);
    bsp_DelayUS(EXPH743_CS_SETUP_US);

    status = HAL_SPI_TransmitReceive(&hspi1,
                                     s_spi_tx_buf,
                                     s_spi_rx_buf,
                                     EXPH743_SPI_FRAME_SIZE,
                                     EXPH743_SPI_TIMEOUT_MS);

    bsp_DelayUS(EXPH743_CS_SETUP_US);
    HAL_GPIO_WritePin(EXPH743_CS_PORT, EXPH743_CS_PIN, GPIO_PIN_SET);

    s_state.last_spi_status = status;
    if (status == HAL_OK)
    {
        ExpH743Spi_ParseFrame();
        s_state.frame_count++;
    }

    return status;
}

const ExpH743SpiState_t *ExpH743Spi_GetState(void)
{
    return &s_state;
}

uint16_t ExpH743Spi_BuildFmcuPiValue(uint8_t mode, const uint8_t payload_bytes[4])
{
    const uint8_t control_byte_one = payload_bytes[0];
    const uint8_t control_byte_two = payload_bytes[1];
    const uint8_t control_byte_three = payload_bytes[2];
    const uint8_t control_byte_four = payload_bytes[3];
    uint16_t value = 0x0000U;

    switch (mode)
    {
        case 0x0U:
            value |= (uint16_t)control_byte_one;
            value |= (uint16_t)((control_byte_two & 0x01U) << 8);
            value |= (uint16_t)(((control_byte_two >> 4) & 0x01U) << 9);
            value |= (uint16_t)((control_byte_three & 0x01U) << 10);
            value |= (uint16_t)(((control_byte_three >> 4) & 0x01U) << 11);
            value |= (uint16_t)((control_byte_four & 0x01U) << 12);
            value |= (uint16_t)(((control_byte_four >> 4) & 0x01U) << 13);
            break;

        case 0x6U:
            value = (uint16_t)control_byte_one;
            break;

        case 0x1U:
            value |= (uint16_t)control_byte_two;
            value |= (uint16_t)((uint16_t)control_byte_one << 8);
            break;

        case 0x2U:
        case 0x3U:
            value |= (uint16_t)((control_byte_one & 0x01U) << 0);
            value |= (uint16_t)(((control_byte_one >> 4) & 0x01U) << 1);
            value |= (uint16_t)((control_byte_two & 0x01U) << 2);
            value |= (uint16_t)(((control_byte_two >> 4) & 0x01U) << 3);
            value |= (uint16_t)((control_byte_three & 0x01U) << 4);
            value |= (uint16_t)(((control_byte_three >> 4) & 0x01U) << 5);
            value |= (uint16_t)((control_byte_four & 0x01U) << 6);
            value |= (uint16_t)(((control_byte_four >> 4) & 0x01U) << 7);
            break;

        default:
            break;
    }

    return value;
}
