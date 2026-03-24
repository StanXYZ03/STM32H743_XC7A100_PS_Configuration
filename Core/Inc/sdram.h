/*
 * File: sdram.h
 * File Created: Wednesday, 18th March 2026 2:19:50 pm
 * Author: 赵祥宇
 * -----
 * Last Modified: Tuesday, 24th March 2026 2:08:47 pm
 * Modified By: 赵祥宇
 * -----
 * Copyright (c) 2026 北京革新创展科技有限公司
 */

#ifndef __SDRAM_H
#define __SDRAM_H

#include "stm32h7xx_hal.h"
#include "fmc.h"
#include "cmsis_os.h"  // 新增：FreeRTOS头文件

/* -------------------------- 宏定义 -------------------------- */
// SDRAM起始地址（BANK2）
#define SDRAM_BASE_ADDR        ((uint32_t)0xC0A00000)
// SDRAM总大小(512KB)
#define SDRAM_TOTAL_SIZE       ((uint32_t)0x00080000)
// SDRAM缓存bin文件的起始偏移（从0开始）
#define SDRAM_BIN_OFFSET       ((uint32_t)0x000000)
#define SDRAM_TIMEOUT          ((uint32_t)0xFFFFU)

// 协议指令定义
#define CMD_START_BIN         0x5A    // 启动接收bin文件
#define CMD_END_BIN_BYTE1     0x55    // 结束接收字节1
#define CMD_END_BIN_BYTE2     0xAA    // 结束接收字节2
// SDRAM接收状态枚举
typedef enum
{
    SDRAM_RECV_IDLE = 0,    // 空闲（未收到启动指令）
    SDRAM_RECV_READY,       // 已就绪（收到启动指令，等待数据）
    SDRAM_RECV_DATA,        // 正在接收数据
    SDRAM_RECV_COMPLETE     // 接收完成（收到结束指令）
} SDRAM_Recv_State;

// SDRAM模式寄存器配置宏（CL=3）
#define SDRAM_MODEREG_BURST_LENGTH_1             ((uint16_t)0x0000)
#define SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL      ((uint16_t)0x0000)
#define SDRAM_MODEREG_CAS_LATENCY_3              ((uint16_t)0x0030)
#define SDRAM_MODEREG_OPERATING_MODE_STANDARD    ((uint16_t)0x0000)
#define SDRAM_MODEREG_WRITEBURST_MODE_SINGLE     ((uint16_t)0x0200)

/* -------------------------- 全局变量声明 -------------------------- */
// SDRAM缓存bin文件的当前写入偏移（全局变量，USB接收时累加）
extern uint32_t g_sdram_bin_offset;
extern SDRAM_Recv_State g_sdram_recv_state;  // 接收状态

/* -------------------------- 函数声明 -------------------------- */
void SDRAM_Init_Sequence(void);
void SDRAM_WriteBuffer(uint8_t *pBuffer, uint32_t WriteAddr, uint32_t n);
void SDRAM_ReadBuffer(uint8_t *pBuffer, uint32_t ReadAddr, uint32_t n);
HAL_StatusTypeDef SDRAM_SendCmd(uint32_t cmd, uint32_t refresh_num, uint16_t mode_reg_val);
// 新增：SDRAM缓存重置（清空偏移，准备接收新文件）
void SDRAM_Bin_Cache_Reset(void);

#endif /* __SDRAM_H */
