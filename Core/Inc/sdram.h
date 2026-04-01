/*
 * File: sdram.h
 */

#ifndef __SDRAM_H
#define __SDRAM_H

#include "stm32h7xx_hal.h"
#include "fmc.h"
#include "cmsis_os.h"

#define SDRAM_BASE_ADDR        ((uint32_t)0xC0A00000)
#define BRIDGE_BITSTREAM_ADDR  ((uint32_t)0xC0A90000)
#define SDRAM_TOTAL_SIZE       ((uint32_t)0x000B0000)
#define SDRAM_BIN_OFFSET       ((uint32_t)0x000000)
#define SDRAM_TIMEOUT          ((uint32_t)0xFFFFU)

#define CMD_START_BIN          0x5A
#define CMD_END_BIN_BYTE1      0x55
#define CMD_END_BIN_BYTE2      0xAA
#define CMD_END_BIN_BYTE3      0xAA
#define CMD_END_BIN_BYTE4      0x55

typedef enum
{
    SDRAM_RECV_IDLE = 0,
    SDRAM_RECV_READY,
    SDRAM_RECV_DATA,
    SDRAM_RECV_COMPLETE
} SDRAM_Recv_State;

#define SDRAM_MODEREG_BURST_LENGTH_1             ((uint16_t)0x0000)
#define SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL      ((uint16_t)0x0000)
#define SDRAM_MODEREG_CAS_LATENCY_3              ((uint16_t)0x0030)
#define SDRAM_MODEREG_OPERATING_MODE_STANDARD    ((uint16_t)0x0000)
#define SDRAM_MODEREG_WRITEBURST_MODE_SINGLE     ((uint16_t)0x0200)

extern uint32_t g_sdram_bin_offset;
extern SDRAM_Recv_State g_sdram_recv_state;

void SDRAM_Init_Sequence(void);
void SDRAM_WriteBuffer(uint8_t *pBuffer, uint32_t WriteAddr, uint32_t n);
void SDRAM_ReadBuffer(uint8_t *pBuffer, uint32_t ReadAddr, uint32_t n);
HAL_StatusTypeDef SDRAM_SendCmd(uint32_t cmd, uint32_t refresh_num, uint16_t mode_reg_val);
void SDRAM_Bin_Cache_Reset(void);

#endif
