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
#include <stdbool.h>


/* 引脚配置（不变） */
#if CONFIGURATION_MODE
#define FPGA_CCLK_PORT    GPIOE
#define FPGA_CCLK_PIN     GPIO_PIN_2
#define FPGA_DATA0_PORT   GPIOE
#define FPGA_DATA0_PIN    GPIO_PIN_6
#else
#define JTAG_GPIO_PORT         GPIOE
#define JTAG_TCK_PIN      GPIO_PIN_2
#define JTAG_TDI_PIN      GPIO_PIN_6
#define JTAG_TDO_PIN      GPIO_PIN_5
#define JTAG_TMS_PIN      GPIO_PIN_4

// ================= Xilinx Artix-7 专用定义 =================
#define XILINX_INST_JPROGRAM    0x0B    // JTAG 指令: 启动 FPGA 配置
#define XILINX_INST_CFG_IN      0x05    // JTAG 指令: 输入配置数据
#define XILINX_INST_JSTART      0x0C    // JTAG 指令: 启动配置序列
#define XILINX_INST_BYPASS      0x3F    // JTAG 指令: 旁路 (默认)
#define XILINX_INST_IDCODE      0x09    // JTAG 指令: 读取 IDCODE

// 时钟使能宏 (针对 GPIOE)
#define JTAG_GPIO_CLK_ENABLE()     __HAL_RCC_GPIOE_CLK_ENABLE()
#endif

#define FPGA_PROGB_PORT   GPIOE
#define FPGA_PROGB_PIN    GPIO_PIN_3
#define FPGA_INITB_PORT   GPIOC
#define FPGA_INITB_PIN    GPIO_PIN_13
#define FPGA_DONE_PORT    GPIOC
#define FPGA_DONE_PIN     GPIO_PIN_14

#define XILINX_IR_LEN           6       // Artix-7 指令寄存器长度: 6 bits
#define JTAG_FREQ_DEFAULT_KHZ   800     // 默认 JTAG 频率 
#define JTAG_BUFFER_SIZE        4096    // 数据缓冲区大小

/* 协议指令定义（新增） */
#define CMD_START_FPGA_CONFIG_BYTE1 0xf0  // 第一字节
#define CMD_START_FPGA_CONFIG_BYTE2 0x0f  // 第二字节
#define CCLK_HIGH_MIN_NS    30  // CCLK最小高电平时间（取手册上限，更稳定）
#define CCLK_LOW_MIN_NS     30  // CCLK最小低电平时间
#define DATA_SETUP_MIN_NS   20  // DATA0最小建立时间（CCLK上升沿前）
#define DATA_HOLD_MIN_NS    20  // DATA0最小保持时间（CCLK上升沿后）
#define PROGB_LOW_DELAY      2

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

typedef enum {
    JTAG_TLR = 0,  // Test-Logic-Reset
    JTAG_RTI,      // Run-Test/Idle
    JTAG_SDS,      // Select-DR-Scan
    JTAG_CDR,      // Capture-DR
    JTAG_SDR,      // Shift-DR
    JTAG_E1D,      // Exit1-DR
    JTAG_PDR,      // Pause-DR
    JTAG_E2D,      // Exit2-DR
    JTAG_UDR,      // Update-DR
    JTAG_SIS,      // Select-IR-Scan
    JTAG_CIR,      // Capture-IR
    JTAG_SIR,      // Shift-IR
    JTAG_E1I,      // Exit1-IR
    JTAG_PIR,      // Pause-IR
    JTAG_E2I,      // Exit2-IR
    JTAG_UIR,      // Update-IR
    JTAG_STATE_COUNT
} JtagState;

// ================= 底层硬件抽象层 (HAL) 回调函数 =================
typedef struct {
    void (*SetTMS)(bool level);   // 设置 TMS 引脚电平
    void (*SetTCK)(bool level);   // 设置 TCK 引脚电平
    void (*SetTDI)(bool level);   // 设置 TDI 引脚电平
    bool (*ReadTDO)(void);        // 读取 TDO 引脚电平
} JtagHalCallbacks;

// ================= JTAG 传输请求结构体 =================
typedef struct {
    uint8_t  buffer[JTAG_BUFFER_SIZE]; // 数据缓冲区
    uint32_t bit_count;                 // 要传输的总位数
} JtagTransfer;

// ================= JTAG 接口上下文结构体 (核心控制块) =================
typedef struct {
    JtagState          current_state;   // 当前状态机状态
    bool               last_bit_held;   // 是否暂存了最后一位 (用于状态切换)
    uint8_t            last_bit;        // 暂存的最后一位数据
    JtagHalCallbacks   *hal;            // 指向硬件操作函数的指针
    JtagTransfer       *xfer;           // 指向当前传输数据的指针
} JtagContext;

/* 全局变量（废弃原缓冲区，新增SDRAM相关） */
extern FPGA_StateTypeDef g_fpga_state;
extern uint8_t g_fpga_config_start; // FPGA配置启动标志

extern uint8_t INIT_STATE;
extern uint8_t PROG_STATE;
extern uint8_t DONE_STATE;
extern 	uint32_t send_total;

/* 函数声明 */
void FPGA_Delay_NS(uint32_t ns);
HAL_StatusTypeDef FPGA_Send_Bin_From_SDRAM(uint32_t bin_size); // 从SDRAM发送
void FPGA_Reset(void);
void FPGA_Check_Config_Cmd(uint8_t* buf, uint32_t len); // 检测配置指令

void Jtag_Init(JtagContext *jtag, JtagHalCallbacks *hal, JtagTransfer *xfer);
void Jtag_Reset(JtagContext *jtag);
void Jtag_GotoState(JtagContext *jtag, JtagState target_state);
void Jtag_RunClocks(JtagContext *jtag, uint32_t count, JtagState end_state);
void Jtag_WriteInstruction(JtagContext *jtag, uint8_t inst, JtagState end_state);
void Jtag_WriteData(JtagContext *jtag, const uint8_t *data, uint32_t bit_len, JtagState end_state);
void Jtag_ReadData(JtagContext *jtag, uint8_t *data, uint32_t bit_len, JtagState end_state);

/**
 * @brief 返回 HAL 操作句柄
 */
JtagHalCallbacks* BSP_Jtag_GetHalOps(void);

#endif // __FPGA_CONFIG_H
