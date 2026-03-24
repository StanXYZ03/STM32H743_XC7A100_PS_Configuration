/*
 * File: fpga_config.h
 * File Created: Monday, 16th March 2026 1:15:21 pm
 * Author: 赵祥宇
 * -----
 * Last Modified: Tuesday, 24th March 2026 2:08:31 pm
 * Modified By: 赵祥宇
 * -----
 * Copyright (c) 2026 北京革新创展科技有限公司
 */


#ifndef __FPGA_CONFIG_H
#define __FPGA_CONFIG_H

#include "stm32h7xx_hal.h"
#include "usbd_cdc_if.h"
#include "stdint.h"
#include "string.h"
#include "stdio.h"
#include "inttypes.h"
#include "sdram.h"  // 引入SDRAM相关定义
#include "spi.h"
#include "dma.h"

/* 引脚配置（不变） */
#define FPGA_CCLK_PORT    GPIOE
#define FPGA_CCLK_PIN     GPIO_PIN_2
#define FPGA_DATA0_PORT   GPIOE
#define FPGA_DATA0_PIN    GPIO_PIN_6
#define FPGA_PROGB_PORT   GPIOE
#define FPGA_PROGB_PIN    GPIO_PIN_3
#define FPGA_INITB_PORT   GPIOC
#define FPGA_INITB_PIN    GPIO_PIN_13
#define FPGA_DONE_PORT    GPIOC
#define FPGA_DONE_PIN     GPIO_PIN_14

/* 协议指令定义（新增） */
#define CMD_START_FPGA_CONFIG_BYTE1 0xf0  // 第一字节
#define CMD_START_FPGA_CONFIG_BYTE2 0x0f  // 第二字节
#define CCLK_HIGH_MIN_NS    30  // CCLK最小高电平时间（取手册上限，更稳定）
#define CCLK_LOW_MIN_NS     30  // CCLK最小低电平时间
#define DATA_SETUP_MIN_NS   20  // DATA0最小建立时间（CCLK上升沿前）
#define DATA_HOLD_MIN_NS    20  // DATA0最小保持时间（CCLK上升沿后）
#define PROGB_LOW_DELAY       2

/* 状态枚举（新增配置状态） */
typedef enum {
    FPGA_STATE_IDLE = 0,
    FPGA_STATE_RESET,
    FPGA_STATE_WAIT_INITB,
    FPGA_STATE_SENDING,
    FPGA_STATE_WAIT_DONE,
    FPGA_STATE_SUCCESS,
    FPGA_STATE_FAILED,
    FPGA_STATE_READY_FOR_CONFIG // 准备配置状态
} FPGA_StateTypeDef;

/* 全局变量（废弃原缓冲区，新增SDRAM相关） */
extern FPGA_StateTypeDef g_fpga_state;
extern uint8_t g_fpga_config_start; // FPGA配置启动标志

extern uint8_t INIT_STATE;
extern uint8_t PROG_STATE;
extern uint8_t DONE_STATE;
extern 	uint32_t send_total;

/* 函数声明（重构） */
void FPGA_Delay_NS(uint32_t ns);
HAL_StatusTypeDef FPGA_Send_Bin_From_SDRAM(uint32_t bin_size); // 从SDRAM发送
void FPGA_Reset(void);
void FPGA_Check_Config_Cmd(uint8_t* buf, uint32_t len); // 检测配置指令
void FPGA_Print_Sync_Word(void);

uint8_t* FPGA_Find_Sync_Word(void);

#endif
