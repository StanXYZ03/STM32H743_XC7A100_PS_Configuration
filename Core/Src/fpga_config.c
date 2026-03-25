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
JtagHalCallbacks jtag_hal_ops;
uint8_t g_fpga_config_start = 0; // FPGA配置启动标志

uint8_t INIT_STATE = 0;
uint8_t PROG_STATE = 0;
uint8_t DONE_STATE = 0;
uint32_t send_total = 0;
/* 私有函数（不变） */
static HAL_StatusTypeDef FPGA_Wait_InitB_Ready(void);
static HAL_StatusTypeDef FPGA_Wait_DONE_High(void);

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

// 状态机跳转查找表 (TMS 序列 LSB 优先)
// 索引: [当前状态][目标状态]
static const uint8_t jtag_tms_lut[JTAG_STATE_COUNT][JTAG_STATE_COUNT] = {
    // TLR   RTI   SDS   CDR   SDR   E1D   PDR   E2D   UDR   SIS   CIR   SIR   E1I   PIR   E2I   UIR
    {0x00, 0x00, 0x02, 0x02, 0x02, 0x0A, 0x0A, 0x2A, 0x1A, 0x06, 0x06, 0x06, 0x16, 0x16, 0x56, 0x36}, // TLR
    {0x07, 0x00, 0x01, 0x01, 0x01, 0x05, 0x05, 0x15, 0x0D, 0x03, 0x03, 0x03, 0x0B, 0x0B, 0x2B, 0x1B}, // RTI
    {0x03, 0x03, 0x00, 0x00, 0x00, 0x02, 0x02, 0x0A, 0x06, 0x01, 0x01, 0x01, 0x05, 0x05, 0x15, 0x0D}, // SDS
    {0x1F, 0x03, 0x07, 0x00, 0x00, 0x01, 0x01, 0x05, 0x03, 0x0F, 0x0F, 0x0F, 0x2F, 0x2F, 0xAF, 0x6F}, // CDR
    {0x1F, 0x03, 0x07, 0x07, 0x00, 0x01, 0x01, 0x05, 0x03, 0x0F, 0x0F, 0x0F, 0x2F, 0x2F, 0xAF, 0x6F}, // SDR
    {0x0F, 0x01, 0x03, 0x03, 0x02, 0x00, 0x00, 0x02, 0x01, 0x07, 0x07, 0x07, 0x17, 0x17, 0x57, 0x37}, // E1D
    {0x1F, 0x03, 0x07, 0x07, 0x01, 0x05, 0x00, 0x01, 0x03, 0x0F, 0x0F, 0x0F, 0x2F, 0x2F, 0xAF, 0x6F}, // PDR
    {0x0F, 0x01, 0x03, 0x03, 0x00, 0x02, 0x02, 0x00, 0x01, 0x07, 0x07, 0x07, 0x17, 0x17, 0x57, 0x37}, // E2D
    {0x07, 0x00, 0x01, 0x01, 0x01, 0x05, 0x05, 0x15, 0x00, 0x03, 0x03, 0x03, 0x0B, 0x0B, 0x2B, 0x1B}, // UDR
    {0x01, 0x01, 0x05, 0x05, 0x05, 0x15, 0x15, 0x55, 0x35, 0x00, 0x00, 0x00, 0x02, 0x02, 0x0A, 0x06}, // SIS
    {0x1F, 0x03, 0x07, 0x07, 0x07, 0x17, 0x17, 0x57, 0x37, 0x0F, 0x00, 0x00, 0x01, 0x01, 0x05, 0x03}, // CIR
    {0x1F, 0x03, 0x07, 0x07, 0x07, 0x17, 0x17, 0x57, 0x37, 0x0F, 0x0F, 0x00, 0x01, 0x01, 0x05, 0x03}, // SIR
    {0x0F, 0x01, 0x03, 0x03, 0x03, 0x0B, 0x0B, 0x2B, 0x1B, 0x07, 0x07, 0x02, 0x00, 0x00, 0x02, 0x01}, // E1I
    {0x1F, 0x03, 0x07, 0x07, 0x07, 0x17, 0x17, 0x57, 0x37, 0x0F, 0x0F, 0x01, 0x05, 0x00, 0x01, 0x03}, // PIR
    {0x0F, 0x01, 0x03, 0x03, 0x03, 0x0B, 0x0B, 0x2B, 0x1B, 0x07, 0x07, 0x00, 0x02, 0x02, 0x00, 0x01}, // E2I
    {0x07, 0x00, 0x01, 0x01, 0x01, 0x05, 0x05, 0x15, 0x0D, 0x03, 0x03, 0x03, 0x0B, 0x0B, 0x2B, 0x00}  // UIR
};

// 状态机跳转步数查找表
static const uint8_t jtag_steps_lut[JTAG_STATE_COUNT][JTAG_STATE_COUNT] = {
    {0, 1, 2, 3, 4, 4, 5, 6, 5, 3, 4, 5, 5, 6, 7, 6}, // TLR
    {3, 0, 1, 2, 3, 3, 4, 5, 4, 2, 3, 4, 4, 5, 6, 5}, // RTI
    {2, 3, 0, 1, 2, 2, 3, 4, 3, 1, 2, 3, 3, 4, 5, 4}, // SDS
    {5, 3, 3, 0, 1, 1, 2, 3, 2, 4, 5, 6, 6, 7, 8, 7}, // CDR
    {5, 3, 3, 4, 0, 1, 2, 3, 2, 4, 5, 6, 6, 7, 8, 7}, // SDR
    {4, 2, 2, 3, 3, 0, 1, 2, 1, 3, 4, 5, 5, 6, 7, 6}, // E1D
    {5, 3, 3, 4, 2, 3, 0, 1, 2, 4, 5, 6, 6, 7, 8, 7}, // PDR
    {4, 2, 2, 3, 1, 2, 3, 0, 1, 3, 4, 5, 5, 6, 7, 6}, // E2D
    {3, 1, 1, 2, 3, 3, 4, 5, 0, 2, 3, 4, 4, 5, 6, 5}, // UDR
    {1, 2, 3, 4, 5, 5, 6, 7, 6, 0, 1, 2, 2, 3, 4, 3}, // SIS
    {5, 3, 3, 4, 5, 5, 6, 7, 6, 4, 0, 1, 1, 2, 3, 2}, // CIR
    {5, 3, 3, 4, 5, 5, 6, 7, 6, 4, 5, 0, 1, 2, 3, 2}, // SIR
    {4, 2, 2, 3, 4, 4, 5, 6, 5, 3, 4, 3, 0, 1, 2, 1}, // E1I
    {5, 3, 3, 4, 5, 5, 6, 7, 6, 4, 5, 2, 3, 0, 1, 2}, // PIR
    {4, 2, 2, 3, 4, 4, 5, 6, 5, 3, 4, 1, 2, 3, 0, 1}, // E2I
    {3, 1, 1, 2, 3, 3, 4, 5, 4, 2, 3, 4, 4, 5, 6, 0}  // UIR
};


/**
 * @brief 产生一个 TCK 上升沿，发送一个 Bit
 * @note  时序：先设置 TMS/TDI，再拉高 TCK (采样)，再拉低
 */
static void Jtag_Tick(JtagContext *jtag, bool tms, bool tdi)
{
    jtag->hal->SetTMS(tms);
    jtag->hal->SetTDI(tdi);
    
    FPGA_Delay_NS(100); // 建立时间
    
    jtag->hal->SetTCK(true);  // 上升沿：FPGA 采样 TMS/TDI
    
    FPGA_Delay_NS(100); // 保持时间
    
    jtag->hal->SetTCK(false); // 下降沿：FPGA 更新 TDO
}

// ================= 公共 API 实现 =================

void Jtag_Init(JtagContext *jtag, JtagHalCallbacks *hal, JtagTransfer *xfer)
{
    jtag->hal = hal;
    jtag->xfer = xfer;
    jtag->current_state = JTAG_TLR;
    jtag->last_bit_held = false;
    jtag->last_bit = 0;
    
    // 初始复位
    Jtag_Reset(jtag);
}

void Jtag_Reset(JtagContext *jtag)
{
    // 连续发送 5 个 TMS=1，确保回到 TLR
    for(int i = 0; i < 5; i++) {
        Jtag_Tick(jtag, true, true);
    }
    jtag->current_state = JTAG_TLR;
}

void Jtag_GotoState(JtagContext *jtag, JtagState target_state)
{
    if (jtag->current_state == target_state) return;

    uint8_t tms_seq = jtag_tms_lut[jtag->current_state][target_state];
    uint8_t steps = jtag_steps_lut[jtag->current_state][target_state];

    while (steps--) {
        bool tms = (tms_seq & 0x01);
        // 状态跳转时，TDI 可以是任意值 (通常给 1)
        Jtag_Tick(jtag, tms, true);
        tms_seq >>= 1;
    }
    
    jtag->current_state = target_state;
}

void Jtag_RunClocks(JtagContext *jtag, uint32_t count, JtagState end_state)
{
    Jtag_GotoState(jtag, JTAG_RTI);
    while (count--) {
        // TMS=0，保持在 RTI 或 DR/IR 移位状态
        Jtag_Tick(jtag, false, true); 
    }
    Jtag_GotoState(jtag, end_state);
}

/**
 * @brief 底层移位函数 (仅在 SIR/SDR 状态下调用)
 */
static void Jtag_ShiftRaw(JtagContext *jtag, const uint8_t *tx_data, uint8_t *rx_data, uint32_t bit_len, bool end_after_shift)
{
    uint32_t bits_remaining = bit_len;
    uint32_t byte_idx = 0;
    uint8_t bit_idx = 0;
    uint8_t rx_byte = 0;

    // 1. 处理前 N-1 个 Bit
    while (bits_remaining > 1) {
        bool tdi = false;
        if (tx_data) {
            tdi = (tx_data[byte_idx] >> bit_idx) & 0x01;
        }

        // TMS=0，保持在 Shift 状态
        Jtag_Tick(jtag, false, tdi);

        // 读取 TDO
        if (rx_data) {
            bool tdo = jtag->hal->ReadTDO();
            rx_byte |= (tdo << bit_idx);
        }

        bit_idx++;
        if (bit_idx == 8) {
            if (rx_data) rx_data[byte_idx] = rx_byte;
            rx_byte = 0;
            bit_idx = 0;
            byte_idx++;
        }
        bits_remaining--;
    }

    // 2. 处理最后 1 个 Bit
    if (bits_remaining == 1) {
        bool tdi = false;
        if (tx_data) {
            tdi = (tx_data[byte_idx] >> bit_idx) & 0x01;
        }

        // 如果需要结束，最后一个 Bit 时 TMS=1，进入 Exit1 状态
        bool tms = end_after_shift;
        
        Jtag_Tick(jtag, tms, tdi);

        if (rx_data) {
            bool tdo = jtag->hal->ReadTDO();
            rx_byte |= (tdo << bit_idx);
            rx_data[byte_idx] = rx_byte; // 保存最后一个字节
        }

        // 更新状态
        if (end_after_shift) {
            // 此时已经在 Exit1 了
            jtag->current_state = (jtag->current_state == JTAG_SDR) ? JTAG_E1D : JTAG_E1I;
        }
    }
}

void Jtag_WriteInstruction(JtagContext *jtag, uint8_t inst, JtagState end_state)
{
    // 1. 进入 Shift-IR
    Jtag_GotoState(jtag, JTAG_SIR);

    // 2. 准备数据 (注意：Xilinx 通常 LSB 优先)
    // 这里的 buffer 只是临时用，因为指令很短
    uint8_t inst_buf[2] = {inst, 0};
    
    // 3. 移位
    // 如果 end_state 不是 SIR，说明要退出，那么 ShiftRaw 会在最后一拍拉高 TMS
    bool exit_after = (end_state != JTAG_SIR);
    Jtag_ShiftRaw(jtag, inst_buf, NULL, XILINX_IR_LEN, exit_after);

    // 4. 进入最终状态 (通常是 RTI)
    Jtag_GotoState(jtag, end_state);
}

void Jtag_WriteData(JtagContext *jtag, const uint8_t *data, uint32_t bit_len, JtagState end_state)
{
    // 1. 进入 Shift-DR
    Jtag_GotoState(jtag, JTAG_SDR);

    // 2. 移位数据
    bool exit_after = (end_state != JTAG_SDR);
    Jtag_ShiftRaw(jtag, data, NULL, bit_len, exit_after);

    // 3. 进入最终状态
    // 如果 end_state 是 SDR，说明是连续传输，不跳转
    if (end_state != JTAG_SDR) {
        Jtag_GotoState(jtag, end_state);
    }
}

void Jtag_ReadData(JtagContext *jtag, uint8_t *data, uint32_t bit_len, JtagState end_state)
{
    // 1. 进入 Shift-DR
    Jtag_GotoState(jtag, JTAG_SDR);

    // 2. 移位读取 (TDI 保持高电平，即发送 0xFF)
    bool exit_after = (end_state != JTAG_SDR);
    Jtag_ShiftRaw(jtag, NULL, data, bit_len, exit_after); // tx_data 为 NULL 时默认发 1

    // 3. 进入最终状态
    Jtag_GotoState(jtag, end_state);
}

JtagHalCallbacks* BSP_Jtag_GetHalOps(void)
{
    return &jtag_hal_ops;
}
