/*
 * File: sdram.c
 * File Created: Wednesday, 18th March 2026 2:20:11 pm
 * Author: 赵祥宇
 * -----
 * Last Modified: Wednesday, 18th March 2026 2:20:19 pm
 * Modified By: 赵祥宇
 * -----
 * Copyright (c) 2026 北京革新创展科技有限公司
 */

#include "sdram.h"
#include "cmsis_os.h"
// SDRAM接收相关全局变量
uint32_t g_sdram_bin_offset = 0;
SDRAM_Recv_State g_sdram_recv_state = SDRAM_RECV_IDLE;  // 默认空闲

/**
 * @brief  SDRAM初始化序列
 */
//void SDRAM_Init_Sequence(void)
//{
//    uint32_t mode_reg_val = 0;
//    
//    SDRAM_SendCmd(FMC_SDRAM_CMD_CLK_ENABLE, 1, 0);
//    osDelay(1);
//    
//    SDRAM_SendCmd(FMC_SDRAM_CMD_PALL, 1, 0);
//    SDRAM_SendCmd(FMC_SDRAM_CMD_AUTOREFRESH_MODE, 8, 0);
//    
//    mode_reg_val = SDRAM_MODEREG_BURST_LENGTH_1           |
//                   SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL   |
//                   SDRAM_MODEREG_CAS_LATENCY_3           |
//                   SDRAM_MODEREG_OPERATING_MODE_STANDARD |
//                   SDRAM_MODEREG_WRITEBURST_MODE_SINGLE;
//    SDRAM_SendCmd(FMC_SDRAM_CMD_LOAD_MODE, 1, mode_reg_val);
//    
//    HAL_SDRAM_ProgramRefreshRate(&hsdram1, 1152);
//}
void SDRAM_Init_Sequence(void)
{
    FMC_SDRAM_CommandTypeDef command = {0};
    uint32_t tmpmrd = 0;
		uint32_t tmpr = 0;
    /* Step1: 发送时钟使能命令 */
    command.CommandMode = FMC_SDRAM_CMD_CLK_ENABLE;
    command.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
    command.AutoRefreshNumber = 1;
    command.ModeRegisterDefinition = 0;
    if(HAL_SDRAM_SendCommand(&hsdram1, &command, 0xFFFF) != HAL_OK)
    {
        Error_Handler(); // 初始化失败，触发错误
    }

    /* Step 2: 延时≥100us（案例用100ms，更稳定） */
    osDelay(1);

    /* Step 3: 发送预充电所有Bank命令 */
    command.CommandMode = FMC_SDRAM_CMD_PALL;
		command.CommandMode = FMC_SDRAM_CMD_AUTOREFRESH_MODE;
    command.AutoRefreshNumber = 8;
    if(HAL_SDRAM_SendCommand(&hsdram1, &command, 0xFFFF) != HAL_OK)
    {
        Error_Handler();
    }

    /* Step 4: 加载模式寄存器（案例验证过的保守值） */
    tmpmrd = (uint32_t)SDRAM_MODEREG_BURST_LENGTH_1 |
             SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL |
             SDRAM_MODEREG_CAS_LATENCY_3 |
             SDRAM_MODEREG_OPERATING_MODE_STANDARD |
             SDRAM_MODEREG_WRITEBURST_MODE_SINGLE;

    command.CommandMode = FMC_SDRAM_CMD_LOAD_MODE;
    command.ModeRegisterDefinition = tmpmrd;
    if(HAL_SDRAM_SendCommand(&hsdram1, &command, 0xFFFF) != HAL_OK)
    {
        Error_Handler();
    }

    /* Step 6: 计算并设置刷新频率（案例公式） */
    // SDRAM_CLOCK = 100MHz（先降频，稳定后再调150MHz）
    tmpr = (uint32_t)((100000000 / 1000) * 7.8125);
    HAL_SDRAM_ProgramRefreshRate(&hsdram1, tmpr);
}

/**
 * @brief  SDRAM发送命令
 */
HAL_StatusTypeDef SDRAM_SendCmd(uint32_t cmd, uint32_t refresh_num, uint16_t mode_reg_val)
{
    FMC_SDRAM_CommandTypeDef sdram_cmd;
    
    sdram_cmd.CommandMode = cmd;
    sdram_cmd.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK2;
    sdram_cmd.AutoRefreshNumber = refresh_num;
    sdram_cmd.ModeRegisterDefinition = mode_reg_val;
    
    return HAL_SDRAM_SendCommand(&hsdram1, &sdram_cmd, 1000);
}

/**
 * @brief  SDRAM字节写入（安全版：逐字节写入，避免对齐错误）
 */
void SDRAM_WriteBuffer(uint8_t *pBuffer, uint32_t WriteAddr, uint32_t n)
{
    // 1. 空指针/越界检查（核心：防止非法地址访问）
    if(pBuffer == NULL || WriteAddr >= SDRAM_TOTAL_SIZE || (WriteAddr + n) > SDRAM_TOTAL_SIZE)
    {
        return;
    }
    
    // 2. 等待SDRAM空闲（增加超时，避免死等）
    uint32_t timeout = 1000;
    while (HAL_SDRAM_GetState(&hsdram1) != HAL_SDRAM_STATE_READY && timeout--)
    {
        osDelay(1);
    }
    if(timeout == 0)
    {
        return; // 超时退出，避免死循环
    }
    
    // 3. 逐字节写入（安全，无对齐问题）
    uint8_t* p_sdram = (uint8_t*)(SDRAM_BASE_ADDR + WriteAddr);
    for(uint32_t i = 0; i < n; i++)
    {
        *p_sdram++ = *pBuffer++;
    }
}

/**
 * @brief  SDRAM字节读取（同步修复，避免读取时的对齐错误）
 */
void SDRAM_ReadBuffer(uint8_t *pBuffer, uint32_t ReadAddr, uint32_t n)
{
    if(pBuffer == NULL || ReadAddr >= SDRAM_TOTAL_SIZE || (ReadAddr + n) > SDRAM_TOTAL_SIZE)
    {
        return;
    }
    
    uint32_t timeout = 1000;
    while (HAL_SDRAM_GetState(&hsdram1) != HAL_SDRAM_STATE_READY && timeout--)
    {
        osDelay(1);
    }
    if(timeout == 0)
    {
        return;
    }
    
    uint8_t* p_sdram = (uint8_t*)(SDRAM_BASE_ADDR + ReadAddr);
    for(uint32_t i = 0; i < n; i++)
    {
        *pBuffer++ = *p_sdram++;
    }
}

/**
 * @brief  重置SDRAM bin缓存
 */
void SDRAM_Bin_Cache_Reset(void)
{
    g_sdram_bin_offset = 0;
    g_sdram_recv_state = SDRAM_RECV_IDLE;  // 重置状态为空闲
}
