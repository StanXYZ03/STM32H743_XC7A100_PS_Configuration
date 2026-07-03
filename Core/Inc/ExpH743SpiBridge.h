#ifndef __EXPH743_SPI_BRIDGE_H__
#define __EXPH743_SPI_BRIDGE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define EXPH743_SPI_FRAME_SIZE 12U

/*
 * Ported from Keil_H743_Project main.c.
 * This module only handles the STM32-side SPI frame exchange and frame parsing.
 * It does NOT take over LCD/UI ownership, so it can be merged safely first.
 *
 * Important:
 *   - It uses SPI1 with PA15 as the single software CS line.
 *   - Current mousekeyDefaultTask also uses SPI1/PA15/PI3 for another protocol.
 *   - Therefore this bridge should only be actively polled when the mousekey path
 *     is disabled or when the two protocols are mutually scheduled by upper logic.
 */
typedef struct
{
    uint8_t clk_sel;
    uint8_t mode;
    uint8_t payload_bytes[4];
    uint8_t payload_nibbles[8];
    uint16_t rx_word0;
    uint16_t rx_word1;
    uint16_t rx_word2;
    uint16_t fmcu_pi;
    uint32_t xc7a_po;
    uint16_t xc7a_pio;
    HAL_StatusTypeDef last_spi_status;
    uint32_t frame_count;
} ExpH743SpiState_t;

void ExpH743Spi_Init(void);
HAL_StatusTypeDef ExpH743Spi_Transfer(void);
const ExpH743SpiState_t *ExpH743Spi_GetState(void);
uint16_t ExpH743Spi_BuildFmcuPiValue(uint8_t mode, const uint8_t payload_bytes[4]);

#ifdef __cplusplus
}
#endif

#endif /* __EXPH743_SPI_BRIDGE_H__ */
