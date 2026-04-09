/*
 * File: FPGAConfigDefaultTask.h
 * File Created: Monday, 16th March 2026 2:52:20 pm
 * Author: 赵祥宇
 * -----
 * Last Modified: Tuesday, 24th March 2026 2:08:37 pm
 * Modified By: 赵祥宇
 * -----
 * Copyright (c) 2026 北京革新创展科技有限公司
 */

#ifndef __FPGA_CONFIG_DEFAULT_TASK_H
#define __FPGA_CONFIG_DEFAULT_TASK_H

#include "usb_device.h"
#include "cmsis_os.h"
#include "usbd_cdc_if.h"
#include "stdint.h"
#include "string.h"
#include "stdio.h"
#include "inttypes.h"
#include "gpio.h"

/* USB接收配置 */
#define USB_RECV_BUF_SIZE 1024  
extern uint8_t g_usb_recv_buf[USB_RECV_BUF_SIZE];  
extern uint32_t g_usb_recv_len;                    
extern uint8_t g_usb_recv_flag;                    

typedef enum
{
    FPGA_UI_MODE_NONE = 0,
    FPGA_UI_MODE_SLAVE_SERIAL = 1,
    FPGA_UI_MODE_JTAG_SRAM = 2,
    FPGA_UI_MODE_JTAG_FLASH = 3
} FPGA_UI_Mode_t;

typedef enum
{
    FPGA_UI_FLOW_IDLE = 0,
    FPGA_UI_FLOW_WAIT_BIN,
    FPGA_UI_FLOW_WAIT_MODE,
    FPGA_UI_FLOW_BIN_DONE_WAIT_START,
    FPGA_UI_FLOW_CONFIGURING,
    FPGA_UI_FLOW_SUCCESS,
    FPGA_UI_FLOW_FAILED
} FPGA_UI_FlowState_t;

/* 函数声明 */
void FPGAConfigDefaultTask(void const * argument);
void FPGA_Reset(void);                  // FPGA复位函数
HAL_StatusTypeDef FPGA_Send_Config(uint8_t* data, uint32_t len); // 发送配置数据
int8_t USB_CDC_Recv_Callback(uint8_t* buf, uint32_t* len);
void FPGA_UI_SelectMode(FPGA_UI_Mode_t mode);
FPGA_UI_Mode_t FPGA_UI_GetMode(void);
FPGA_UI_FlowState_t FPGA_UI_GetFlowState(void);
uint32_t FPGA_UI_GetBinSize(void);
void FPGA_UI_RequestStart(void);
void FPGA_UI_ResetSession(void);
#endif
