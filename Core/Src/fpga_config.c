/*
 * File: fpga_config.c
 * File Created: Monday, 16th March 2026 1:14:27 pm
 * Author: 赵祥宇
 * -----
 * Last Modified: Monday, 16th March 2026 1:20:04 pm
 * Modified By: 赵祥宇
 * -----
 * Copyright (c) 2026 北京革新创展科技有限公司
 */

#include "fpga_config.h"

/* 全局变量 */
uint8_t FPGA_BIN_BUF[FPGA_BIN_MAX_SIZE] __attribute__((section(".RAM_D2"))) = {0};
uint32_t FPGA_BIN_LEN = 0;
FPGA_StateTypeDef g_fpga_state = FPGA_STATE_IDLE;
uint8_t g_fpga_recv_complete = 0; // 接收完成标志
uint32_t g_recv_timeout_count = 0; // 动态超时计数器

/* 私有函数（不变） */
static HAL_StatusTypeDef FPGA_Wait_InitB_Ready(void);
static HAL_StatusTypeDef FPGA_Wait_DONE_High(void);
static void FPGA_Send_Byte(uint8_t data);

/* 延时函数（不变） */
void FPGA_Delay_US(uint32_t us)
{
  if(us == 0) return;
  static uint8_t dwt_init = 0;
  if(dwt_init == 0)
  {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    dwt_init = 1;
  }
  uint32_t target = DWT->CYCCNT + (us * (SystemCoreClock / 1000000));
  if(target > DWT->CYCCNT) {
    while(DWT->CYCCNT < target);
  } else {
    while(DWT->CYCCNT > target);
    while(DWT->CYCCNT < target);
  }
}

/* 私有函数实现（不变） */
static HAL_StatusTypeDef FPGA_Wait_InitB_Ready(void)
{
  g_fpga_state = FPGA_STATE_WAIT_INITB;
  uint32_t timeout = 1000;
  while(HAL_GPIO_ReadPin(FPGA_INITB_PORT, FPGA_INITB_PIN) == GPIO_PIN_RESET)
  {
    HAL_Delay(1);
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
  while(HAL_GPIO_ReadPin(FPGA_DONE_PORT, FPGA_DONE_PIN) == GPIO_PIN_RESET)
  {
    HAL_Delay(1);
    if(--timeout == 0) {
      g_fpga_state = FPGA_STATE_FAILED;
      return HAL_TIMEOUT;
    }
  }
  return HAL_OK;
}

static void FPGA_Send_Byte(uint8_t data)
{
  for(uint8_t i = 0; i < 8; i++)
  {
    HAL_GPIO_WritePin(FPGA_DATA0_PORT, FPGA_DATA0_PIN, (data & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    data >>= 1;
    HAL_GPIO_WritePin(FPGA_CCLK_PORT, FPGA_CCLK_PIN, GPIO_PIN_SET);
    FPGA_Delay_US(CCLK_DELAY_US / 2);
    HAL_GPIO_WritePin(FPGA_CCLK_PORT, FPGA_CCLK_PIN, GPIO_PIN_RESET);
    FPGA_Delay_US(CCLK_DELAY_US / 2);
  }
}

/* 回调式数据接收函数（核心：收到新数据就重置超时计数器） */
void FPGA_USB_Recv_Data(uint8_t* buf, uint32_t len)
{
  if(len > 0 && (FPGA_BIN_LEN + len) <= FPGA_BIN_MAX_SIZE)
  {
    // 1. 拷贝真实数据
    memcpy(&FPGA_BIN_BUF[FPGA_BIN_LEN], buf, len);
    FPGA_BIN_LEN += len;
    
    
    // 3. 核心：收到新数据 → 立即重置超时计数器
    g_recv_timeout_count = 0;
    g_fpga_recv_complete = 0;
  }
  else if(len > 0)
  {
    CDC_Transmit_FS((uint8_t*)"[FPGA] Recv data overflow\r\n", 26);
    g_fpga_recv_complete = 0;
  }
}

/* 发送配置数据（不变） */
HAL_StatusTypeDef FPGA_Send_Bin_From_RAM(void)
{
  FPGA_Reset();
  CDC_Transmit_FS((uint8_t*)"[FPGA] FPGA Reset OK\r\n", 18);
  
  if(FPGA_Wait_InitB_Ready() != HAL_OK)
  {
    CDC_Transmit_FS((uint8_t*)"[FPGA] INIT_B timeout\r\n", 24);
    return HAL_TIMEOUT;
  }
  CDC_Transmit_FS((uint8_t*)"[FPGA] INIT_B ready\r\n", 20);
  
  HAL_GPIO_WritePin(FPGA_CCLK_PORT, FPGA_CCLK_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(FPGA_DATA0_PORT, FPGA_DATA0_PIN, GPIO_PIN_RESET);
  
  g_fpga_state = FPGA_STATE_SENDING;
  CDC_Transmit_FS((uint8_t*)"[FPGA] Sending bin data...\r\n", 28);
  for(uint32_t i = 0; i < FPGA_BIN_LEN; i++)
  {
    FPGA_Send_Byte(FPGA_BIN_BUF[i]);
    if(i % 100 == 0)
    {
      char prog_buf[64] = {0};
      snprintf(prog_buf, sizeof(prog_buf), "[FPGA] Progress: %" PRIu32 "/%" PRIu32 "\r\n", i, FPGA_BIN_LEN);
      CDC_Transmit_FS((uint8_t*)prog_buf, strlen(prog_buf));
    }
  }
  
  if(FPGA_Wait_DONE_High() != HAL_OK)
  {
    CDC_Transmit_FS((uint8_t*)"[FPGA] DONE timeout\r\n", 22);
    return HAL_TIMEOUT;
  }
  
  HAL_GPIO_WritePin(FPGA_CCLK_PORT, FPGA_CCLK_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(FPGA_DATA0_PORT, FPGA_DATA0_PIN, GPIO_PIN_SET);
  g_fpga_state = FPGA_STATE_SUCCESS;
  
  CDC_Transmit_FS((uint8_t*)"[FPGA] Config complete\r\n", 24);
  return HAL_OK;
}

/* 复位函数（不变） */
void FPGA_Reset(void)
{
    g_fpga_state = FPGA_STATE_RESET;
    HAL_GPIO_WritePin(FPGA_PROGB_PORT, FPGA_PROGB_PIN, GPIO_PIN_RESET);
    HAL_Delay(2);
    HAL_GPIO_WritePin(FPGA_PROGB_PORT, FPGA_PROGB_PIN, GPIO_PIN_SET);
    HAL_Delay(10);
    g_fpga_state = FPGA_STATE_WAIT_INITB;
}

/* 核心配置函数（重构：动态间隙超时判定） */
void FPGA_Config_Main(void)
{
  // 初始化状态
  FPGA_BIN_LEN = 0;
  memset(FPGA_BIN_BUF, 0, FPGA_BIN_MAX_SIZE);
  g_recv_timeout_count = 0;
  g_fpga_recv_complete = 0;
  
  CDC_Transmit_FS((uint8_t*)"[FPGA] Start receiving bin file (wait 10s after last data)...\r\n", 58);

  // 动态超时循环：只要有新数据就重置计数器，直到10秒无数据
  while(1)
  {
    // 条件1：超时计数器达到10秒 → 判定文件结束
    if(g_recv_timeout_count >= RECV_GAP_TIMEOUT)
    {
      if(FPGA_BIN_LEN > 0)
      {
        CDC_Transmit_FS((uint8_t*)"[FPGA] No new data for 10s, file complete\r\n", 44);
        break;
      }
      else
      {
        CDC_Transmit_FS((uint8_t*)"[FPGA] 10s timeout and no data\r\n", 26);
        g_fpga_state = FPGA_STATE_FAILED;
        return;
      }
    }
    
    // 条件2：未超时 → 累加计数器，等待新数据
    g_recv_timeout_count++;
    osDelay(1); // 1ms累加一次，10000次=10秒
  }

  // 打印最终接收长度
  char total_buf[64] = {0};
  snprintf(total_buf, sizeof(total_buf), "[FPGA] Recv complete, total: %" PRIu32 " bytes\r\n", FPGA_BIN_LEN);
  CDC_Transmit_FS((uint8_t*)total_buf, strlen(total_buf));
  
  // 发送配置数据
  if(FPGA_Send_Bin_From_RAM() != HAL_OK)
  {
    CDC_Transmit_FS((uint8_t*)"[FPGA] Config failed\r\n", 22);
    g_fpga_state = FPGA_STATE_FAILED;
    return;
  }
  
  CDC_Transmit_FS((uint8_t*)"[FPGA] All operations success\r\n", 32);
  g_fpga_recv_complete = 0;
}
