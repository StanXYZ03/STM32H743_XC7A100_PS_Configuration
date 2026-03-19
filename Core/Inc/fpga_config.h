/*
 * File: fpga_config_task.h
 * File Created: Monday, 16th March 2026 1:15:21 pm
 * Author: 赵祥宇
 * -----
 * Last Modified: Monday, 16th March 2026 1:17:01 pm
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

/* 引脚/缓存配置（不变） */
#define FPGA_CCLK_PORT    GPIOG
#define FPGA_CCLK_PIN     GPIO_PIN_12
#define FPGA_DATA0_PORT   GPIOI
#define FPGA_DATA0_PIN    GPIO_PIN_8
#define FPGA_PROGB_PORT   GPIOE
#define FPGA_PROGB_PIN    GPIO_PIN_3
#define FPGA_INITB_PORT   GPIOC
#define FPGA_INITB_PIN    GPIO_PIN_14
#define FPGA_DONE_PORT    GPIOC
#define FPGA_DONE_PIN     GPIO_PIN_13

#define FPGA_BIN_MAX_SIZE 0x40000  // 256KB
#define CCLK_DELAY_US     2
#define PROGB_LOW_DELAY   2

/* 状态枚举（不变） */
typedef enum {
    FPGA_STATE_IDLE = 0,
    FPGA_STATE_RESET,
    FPGA_STATE_WAIT_INITB,
    FPGA_STATE_SENDING,
    FPGA_STATE_WAIT_DONE,
    FPGA_STATE_SUCCESS,
    FPGA_STATE_FAILED
} FPGA_StateTypeDef;

/* 动态超时配置（核心修改） */
#define RECV_GAP_TIMEOUT 10000  // 单次超时窗口：10秒（10000ms），足够发送大文件
extern uint32_t g_recv_timeout_count; // 动态超时计数器

/* 全局变量（新增接收完成标志） */
extern uint8_t FPGA_BIN_BUF[FPGA_BIN_MAX_SIZE];
extern uint32_t FPGA_BIN_LEN;
extern FPGA_StateTypeDef g_fpga_state;
extern uint8_t g_fpga_recv_complete; // 接收完成标志（回调中置位）

/* 函数声明 */
void FPGA_Delay_US(uint32_t us);
HAL_StatusTypeDef FPGA_Send_Bin_From_RAM(void);
void FPGA_Config_Main(void);
void FPGA_Reset(void);
void FPGA_USB_Recv_Data(uint8_t* buf, uint32_t len); // 回调数据处理函数

#endif
