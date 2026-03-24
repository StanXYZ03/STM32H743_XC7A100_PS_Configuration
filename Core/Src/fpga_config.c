/*
 * File: fpga_config.c
 * File Created: Monday, 16th March 2026 1:14:27 pm
 * Author: 赵祥宇
 * -----
 * Last Modified: Tuesday, 24th March 2026 2:07:51 pm
 * Modified By: 赵祥宇
 * -----
 * Copyright (c) 2026 北京革新创展科技有限公司
 */


#include "fpga_config.h"

/* 全局变量 */
FPGA_StateTypeDef g_fpga_state = FPGA_STATE_IDLE;
uint8_t g_fpga_config_start = 0; // FPGA配置启动标志

uint8_t INIT_STATE = 0;
uint8_t PROG_STATE = 0;
uint8_t DONE_STATE = 0;
uint32_t send_total = 0;
/* 私有函数（不变） */
static HAL_StatusTypeDef FPGA_Wait_InitB_Ready(void);
static HAL_StatusTypeDef FPGA_Wait_DONE_High(void);
static void FPGA_Print_Sync_Word(void);
static uint8_t* FPGA_Find_Sync_Word(void);

/**
 * @brief  基于DWT的精准纳秒延时函数（适配480MHz SYSCLK）
 * @param  ns: 要延时的纳秒数（最小支持1ns，最大支持4294967295ns）
 * @note   480MHz主频下，1个时钟周期 = 1/480000000 ≈ 2.0833ns
 */
void FPGA_Delay_NS(uint32_t ns)
{
    if(ns == 0) return; // 避免0延时
    
    static uint8_t dwt_init = 0;
    if(dwt_init == 0)
    {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; // 使能DWT
        DWT->CYCCNT = 0;                                // 重置周期计数器
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;            // 启动计数器
        dwt_init = 1;
    }

    // 480MHz = 480个周期/μs = 0.48个周期/ns → 总周期数 = ns * 0.48
    // 先换算为总时钟周期数（四舍五入保证精度）
    uint64_t total_cycles = (uint64_t)ns * SystemCoreClock / 1000000000;
    uint32_t target = DWT->CYCCNT + (uint32_t)total_cycles;

    // 处理计数器溢出（32位CYCCNT最大值约8.9秒，ns级延时几乎不会溢出，简化处理）
    if(target < DWT->CYCCNT)
    {
        while(DWT->CYCCNT > target); // 等待溢出归零
    }
    while(DWT->CYCCNT < target);     // 等待达到目标周期
}

/* 私有函数实现（不变） */
static HAL_StatusTypeDef FPGA_Wait_InitB_Ready(void)
{
  g_fpga_state = FPGA_STATE_WAIT_INITB;
  uint32_t timeout = 10000;
	INIT_STATE = HAL_GPIO_ReadPin(FPGA_INITB_PORT, FPGA_INITB_PIN);
	PROG_STATE = HAL_GPIO_ReadPin(FPGA_PROGB_PORT, FPGA_PROGB_PIN);
  while(HAL_GPIO_ReadPin(FPGA_INITB_PORT, FPGA_INITB_PIN) == GPIO_PIN_RESET)
  {
    osDelay(1);
    if(--timeout == 0) {
      g_fpga_state = FPGA_STATE_FAILED;
      return HAL_TIMEOUT;
    }
  }
  return HAL_OK;
}

static HAL_StatusTypeDef FPGA_Wait_DONE_High(void)
{
  g_fpga_state = FPGA_STATE_WAIT_DONE;
  uint32_t timeout = 2000;
	DONE_STATE = HAL_GPIO_ReadPin(FPGA_DONE_PORT, FPGA_DONE_PIN);
  while(HAL_GPIO_ReadPin(FPGA_DONE_PORT, FPGA_DONE_PIN) == GPIO_PIN_RESET)
  {
    osDelay(1);

    if(--timeout == 0) {
      g_fpga_state = FPGA_STATE_FAILED;
      return HAL_TIMEOUT;
    }
		if(HAL_GPIO_ReadPin(FPGA_INITB_PORT, FPGA_INITB_PIN) == GPIO_PIN_RESET)
  {
    xTaskResumeAll();
    CDC_Transmit_FS((uint8_t*)"[FPGA] CRC Error! INIT_B dropped (Wait DONE)\r\n", 48);
    return HAL_ERROR;
  }
  }
  return HAL_OK;
}

/* 重构：从SDRAM发送bin数据配置FPGA */
HAL_StatusTypeDef FPGA_Send_Bin_From_SDRAM(uint32_t bin_size)
{
  if(bin_size == 0 || bin_size > SDRAM_TOTAL_SIZE)
  {
    CDC_Transmit_FS((uint8_t*)"[FPGA] Invalid bin size\r\n", 24);
    return HAL_ERROR;
  }
	
	
  // 1. 复位和初始化
  FPGA_Reset();
  CDC_Transmit_FS((uint8_t*)"[FPGA] FPGA Reset OK\r\n", 18);
	
	MX_SPI4_Init();
  osDelay(1);
	
  // 2. 等待INIT_B就绪
  if(FPGA_Wait_InitB_Ready() != HAL_OK)
  {
    CDC_Transmit_FS((uint8_t*)"[FPGA] INIT_B timeout\r\n", 24);
		osDelay(10);
    return HAL_TIMEOUT;
  }  if(HAL_GPIO_ReadPin(FPGA_INITB_PORT, FPGA_INITB_PIN) != GPIO_PIN_SET)
  {
    CDC_Transmit_FS((uint8_t*)"[FPGA] INIT_B abnormal after ready check\r\n", 38);
    g_fpga_state = FPGA_STATE_FAILED;
    return HAL_ERROR;
  }  CDC_Transmit_FS((uint8_t*)"[FPGA] INIT_B ready\r\n", 20);
	osDelay(1);
  
	// 3. 发送数据（SPI+DMA 分块）
  g_fpga_state = FPGA_STATE_SENDING;
  CDC_Transmit_FS((uint8_t*)"[FPGA] Sending bin data via SPI+DMA (one-shot)...\r\n", 48);
  osDelay(10); 

  uint8_t* p_sdram = (uint8_t*)SDRAM_BASE_ADDR;
	uint32_t batch_size = 4096;
  // 禁用FreeRTOS调度器（关键：保证DMA发送不被任务抢占）
  //vTaskSuspendAll();
	while(send_total < bin_size)
  {
  // 检查INIT_B状态（发送前最后确认）
  if(HAL_GPIO_ReadPin(FPGA_INITB_PORT, FPGA_INITB_PIN) == GPIO_PIN_RESET)
  {
    //xTaskResumeAll();
    CDC_Transmit_FS((uint8_t*)"[FPGA] ERROR: INIT_B dropped before send!\r\n", 44);
    g_fpga_state = FPGA_STATE_FAILED;
    return HAL_ERROR;
  }

  // ========== 核心：分块发送bin数据 ==========
	// 计算批次长度
  uint32_t current_batch = (bin_size - send_total) > batch_size ? batch_size : (bin_size - send_total);
  // 启动SPI DMA发送
  if(HAL_SPI_Transmit_DMA(&hspi4, &p_sdram[send_total], current_batch) != HAL_OK)
  {
    //xTaskResumeAll();
    CDC_Transmit_FS((uint8_t*)"[FPGA] ERROR: DMA transmit failed!\r\n", 36);
    g_fpga_state = FPGA_STATE_FAILED;
    HAL_SPI_Abort(&hspi4);
    return HAL_ERROR;
  }

  // 等待DMA发送完成（全程检测INIT_B）
  while(HAL_SPI_GetState(&hspi4) != HAL_SPI_STATE_READY)
  {
    if(HAL_GPIO_ReadPin(FPGA_INITB_PORT, FPGA_INITB_PIN) == GPIO_PIN_RESET)
    {
      HAL_SPI_Abort(&hspi4);
      //xTaskResumeAll();
      CDC_Transmit_FS((uint8_t*)"[FPGA] ERROR: INIT_B dropped (DMA abort)!\r\n", 48);
      g_fpga_state = FPGA_STATE_FAILED;
      return HAL_ERROR;
    }
	}
	// 更新发送进度
	send_total += current_batch;
			// 打印进度（每4096字节一次，无延时）
//			char prog_buf[96] = {0};
//			snprintf(prog_buf, sizeof(prog_buf), "[FPGA] Progress: %" PRIu32 "/%" PRIu32 " (%.2f%%)\r\n", 
//							 send_total, bin_size, (float)send_total / bin_size * 100);
//			CDC_Transmit_FS((uint8_t*)prog_buf, strlen(prog_buf));
	}

	// 4. 发送Dummy CCLK（DMA发送0x00）
	uint8_t dummy_data[100] = {0}; 
	HAL_SPI_Transmit_DMA(&hspi4, dummy_data, 100);
	while(HAL_SPI_GetState(&hspi4) != HAL_SPI_STATE_READY)
	{}
		
	// 恢复FreeRTOS调度器
	//xTaskResumeAll();

  // 5. 等待DONE引脚置高
  if(FPGA_Wait_DONE_High() != HAL_OK)
  {
    CDC_Transmit_FS((uint8_t*)"[FPGA] DONE timeout\r\n", 22);
    return HAL_TIMEOUT;
  }
  
  // 6. 配置完成
  g_fpga_state = FPGA_STATE_SUCCESS;
  
  CDC_Transmit_FS((uint8_t*)"[FPGA] Config complete\r\n", 24);
	osDelay(10);

  return HAL_OK;
}

/* 复位函数（不变） */
void FPGA_Reset(void)
{
    g_fpga_state = FPGA_STATE_RESET;
    HAL_GPIO_WritePin(FPGA_PROGB_PORT, FPGA_PROGB_PIN, GPIO_PIN_RESET);
    osDelay(10);
    HAL_GPIO_WritePin(FPGA_PROGB_PORT, FPGA_PROGB_PIN, GPIO_PIN_SET);
    osDelay(10);
    g_fpga_state = FPGA_STATE_WAIT_INITB;
}

/* 新增：检测FPGA配置启动指令（0x11） */
/* 重构：检测双字节FPGA配置启动指令（0x11+0x99） */
void FPGA_Check_Config_Cmd(uint8_t* buf, uint32_t len)
{
    // 新增：双字节指令缓存+索引
    static uint8_t g_cfg_cmd_buf[2] = {0};  
    static uint8_t g_cfg_cmd_idx = 0;       

    for(uint32_t i = 0; i < len; i++)
    {
        // 步骤1：检测第一字节0x11
        if(g_cfg_cmd_idx == 0 && buf[i] == CMD_START_FPGA_CONFIG_BYTE1)
        {
            g_cfg_cmd_buf[g_cfg_cmd_idx++] = buf[i];
        }
        // 步骤2：检测第二字节0x99
        else if(g_cfg_cmd_idx == 1)
        {
            g_cfg_cmd_buf[g_cfg_cmd_idx++] = buf[i];
            // 校验双字节指令
            if(g_cfg_cmd_buf[0] == CMD_START_FPGA_CONFIG_BYTE1 && 
               g_cfg_cmd_buf[1] == CMD_START_FPGA_CONFIG_BYTE2)
            {
                // 指令校验通过，置位配置启动标志
                g_fpga_config_start = 1; 
                CDC_Transmit_FS((uint8_t*)"[FPGA] Start config cmd received\r\n", 32);
                g_cfg_cmd_idx = 0; // 重置索引，准备下次检测
                break; // 检测到指令后退出循环
            }
            else
            {
                // 指令无效，重置索引（避免残留）
                g_cfg_cmd_idx = 0;
            }
        }
        // 非指令字节，重置索引
        else
        {
            g_cfg_cmd_idx = 0;
        }
    }
}
