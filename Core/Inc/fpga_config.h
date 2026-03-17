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

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "gpio.h"
#include "usbd_cdc.h"
#include "usbd_core.h"
#include "usbd_cdc_if.h"

/* 宏定义 -------------------------------------------------------------------*/
// 1. FPGA配置引脚定义（与你的GPIO初始化对应）
#define FPGA_CCLK_PIN     GPIO_PIN_12
#define FPGA_CCLK_PORT    GPIOG
#define FPGA_DATA0_PIN    GPIO_PIN_8
#define FPGA_DATA0_PORT   GPIOI
#define FPGA_PROGB_PIN    GPIO_PIN_3
#define FPGA_PROGB_PORT   GPIOE
#define FPGA_INITB_PIN    GPIO_PIN_14
#define FPGA_INITB_PORT   GPIOC
#define FPGA_DONE_PIN     GPIO_PIN_13
#define FPGA_DONE_PORT    GPIOC

// 2. RAM缓存配置（XC7A100T的.bin文件最大约200KB，预留256KB足够）
#define FPGA_BIN_MAX_SIZE 0x40000  // 256KB

// 3. Slave Serial时序配置（匹配UG470要求）
#define CCLK_DELAY_US     20       // CCLK周期20us → 频率50kHz（调试阶段低速更稳定）
#define PROGB_LOW_DELAY   1        // PROGRAM_B低电平保持1ms（≥250ns即可）

/* 全局变量 -----------------------------------------------------------------*/
// FPGA配置文件缓存（声明到D2 SRAM，避免栈溢出）
extern uint8_t FPGA_BIN_BUF[FPGA_BIN_MAX_SIZE] __attribute__((section(".RAM_D2")));
extern uint32_t FPGA_BIN_LEN;      // 实际接收的.bin文件长度
extern USBD_HandleTypeDef  hUsbDeviceFS; // USB设备句柄（usbd_conf.c中定义）

/* 函数声明 -----------------------------------------------------------------*/
/**
 * @brief  USB CDC接收FPGA配置.bin文件到内部RAM
 * @retval HAL_OK:成功, HAL_ERROR:失败
 */
HAL_StatusTypeDef FPGA_USB_Recv_Bin_To_RAM(void);

/**
 * @brief  GPIO模拟Slave Serial时序，发送RAM中的.bin文件配置FPGA
 * @retval HAL_OK:成功, HAL_ERROR:失败, HAL_TIMEOUT:超时
 */
HAL_StatusTypeDef FPGA_Send_Bin_From_RAM(void);

/**
 * @brief  FPGA配置主流程（USB接收→RAM缓存→GPIO发送）
 */
void FPGA_Config_Main(void);

/**
 * @brief  微秒级延时函数（用于CCLK时序）
 * @param  us: 延时微秒数
 */
void FPGA_Delay_US(uint32_t us);

/**
 * @brief   FPGA复位函数
 */
 void FPGA_Reset(void); 
#ifdef __cplusplus
}
#endif

#endif /* __FPGA_CONFIG_H */
