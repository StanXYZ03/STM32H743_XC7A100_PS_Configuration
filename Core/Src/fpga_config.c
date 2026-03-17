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

/* 全局变量定义 --------------------------------------------------------------*/
// RAM缓存数组（D2 SRAM）
uint8_t FPGA_BIN_BUF[FPGA_BIN_MAX_SIZE] __attribute__((section(".RAM_D2"))) = {0};
uint32_t FPGA_BIN_LEN = 0;

/* 私有函数声明 --------------------------------------------------------------*/
/**
 * @brief  等待INIT_B引脚拉高（FPGA初始化就绪）
 * @retval HAL_OK:成功, HAL_TIMEOUT:超时
 */
static HAL_StatusTypeDef FPGA_Wait_InitB_Ready(void);

/**
 * @brief  等待DONE引脚拉高（FPGA配置完成）
 * @retval HAL_OK:成功, HAL_TIMEOUT:超时
 */
static HAL_StatusTypeDef FPGA_Wait_DONE_High(void);

/**
 * @brief  模拟SPI时序发送1字节数据（LSB优先，匹配UG470要求）
 * @param  data: 要发送的字节
 */
static void FPGA_Send_Byte(uint8_t data);

/* 函数实现 ------------------------------------------------------------------*/
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
  
  // 修正：480MHz = 480个周期/微秒（原64改为480）
  uint32_t target = DWT->CYCCNT + (us * (SystemCoreClock / 1000000));
  
  if(target > DWT->CYCCNT)
  {
    while(DWT->CYCCNT < target);
  }
  else
  {
    while(DWT->CYCCNT > target);
    while(DWT->CYCCNT < target);
  }
}

static HAL_StatusTypeDef FPGA_Wait_InitB_Ready(void)
{
  uint32_t timeout = 1000; // 超时1000ms
  while(HAL_GPIO_ReadPin(FPGA_INITB_PORT, FPGA_INITB_PIN) == GPIO_PIN_RESET)
  {
    HAL_Delay(1);
    if(--timeout == 0) return HAL_TIMEOUT;
  }
  return HAL_OK;
}

static HAL_StatusTypeDef FPGA_Wait_DONE_High(void)
{
  uint32_t timeout = 1000; // 超时1000ms
  while(HAL_GPIO_ReadPin(FPGA_DONE_PORT, FPGA_DONE_PIN) == GPIO_PIN_RESET)
  {
    HAL_Delay(1);
    if(--timeout == 0) return HAL_TIMEOUT;
  }
  return HAL_OK;
}

static void FPGA_Send_Byte(uint8_t data)
{
  // UG470要求：Slave Serial模式LSB优先发送
  for(uint8_t i = 0; i < 8; i++)
  {
    // 1. 设置DATA0电平（LSB先行）
    if(data & 0x01)
      HAL_GPIO_WritePin(FPGA_DATA0_PORT, FPGA_DATA0_PIN, GPIO_PIN_SET);
    else
      HAL_GPIO_WritePin(FPGA_DATA0_PORT, FPGA_DATA0_PIN, GPIO_PIN_RESET);
    
    // 2. 拉高CCLK，FPGA在上升沿锁存数据
    HAL_GPIO_WritePin(FPGA_CCLK_PORT, FPGA_CCLK_PIN, GPIO_PIN_SET);
    FPGA_Delay_US(CCLK_DELAY_US / 2);
    
    // 3. 拉低CCLK，准备下一位
    HAL_GPIO_WritePin(FPGA_CCLK_PORT, FPGA_CCLK_PIN, GPIO_PIN_RESET);
    FPGA_Delay_US(CCLK_DELAY_US / 2);
    
    // 4. 右移数据，处理下一位
    data >>= 1;
  }
}

HAL_StatusTypeDef FPGA_USB_Recv_Bin_To_RAM(void)
{
  FPGA_BIN_LEN = 0;
  uint8_t temp_buf[512] = {0};
  uint32_t recv_len = 0;

  // 通知上位机准备接收
  CDC_Transmit_FS((uint8_t*)"[FPGA] Ready to receive bin file\r\n", 32);

  // 循环接收数据，直到超时无数据（表示文件发送完成）
  while(1)
  {
    // 使用CubeMX生成的CDC接收函数（usbd_cdc_if.c中实现）
    if(CDC_Receive_FS(temp_buf, &recv_len) != USBD_OK)
    {
      recv_len = 0; // 接收失败，长度置0
    }
    
    // 无数据接收，退出循环
    if(recv_len == 0)
    {
      // 校验接收长度（至少1KB，避免空文件）
      if(FPGA_BIN_LEN < 1024)
      {
        CDC_Transmit_FS((uint8_t*)"[FPGA] Recv data too small\r\n", 28);
        return HAL_ERROR;
      }
      break;
    }

    // 防溢出检查
    if(FPGA_BIN_LEN + recv_len > FPGA_BIN_MAX_SIZE)
    {
      CDC_Transmit_FS((uint8_t*)"[FPGA] RAM buffer overflow\r\n", 30);
      return HAL_ERROR;
    }

    // 拷贝到RAM缓存
    memcpy(&FPGA_BIN_BUF[FPGA_BIN_LEN], temp_buf, recv_len);
    FPGA_BIN_LEN += recv_len;

    // 发送接收确认
    CDC_Transmit_FS((uint8_t*)"[FPGA] Recv: ", 11);
    CDC_Transmit_FS((uint8_t*)&recv_len, 4);
    CDC_Transmit_FS((uint8_t*)" bytes\r\n", 8);
  }

  CDC_Transmit_FS((uint8_t*)"[FPGA] Recv complete, total: ", 30);
  CDC_Transmit_FS((uint8_t*)&FPGA_BIN_LEN, 4);
  CDC_Transmit_FS((uint8_t*)" bytes\r\n", 8);
  
  return HAL_OK;
}

HAL_StatusTypeDef FPGA_Send_Bin_From_RAM(void)
{
  // 步骤1：复位FPGA（拉低PROGRAM_B）
  HAL_GPIO_WritePin(FPGA_PROGB_PORT, FPGA_PROGB_PIN, GPIO_PIN_RESET);
  HAL_Delay(PROGB_LOW_DELAY);
  HAL_GPIO_WritePin(FPGA_PROGB_PORT, FPGA_PROGB_PIN, GPIO_PIN_SET);
  
  // 步骤2：等待INIT_B拉高（FPGA初始化就绪）
  CDC_Transmit_FS((uint8_t*)"[FPGA] Waiting INIT_B ready...\r\n", 32);
  if(FPGA_Wait_InitB_Ready() != HAL_OK)
  {
    CDC_Transmit_FS((uint8_t*)"[FPGA] INIT_B timeout\r\n", 24);
    return HAL_TIMEOUT;
  }
  CDC_Transmit_FS((uint8_t*)"[FPGA] INIT_B ready\r\n", 20);
  
  // 步骤3：初始化CCLK/DATA0为低电平
  HAL_GPIO_WritePin(FPGA_CCLK_PORT, FPGA_CCLK_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(FPGA_DATA0_PORT, FPGA_DATA0_PIN, GPIO_PIN_RESET);
  
  // 步骤4：逐字节发送RAM中的.bin文件（LSB优先）
  CDC_Transmit_FS((uint8_t*)"[FPGA] Sending bin data...\r\n", 28);
  for(uint32_t i = 0; i < FPGA_BIN_LEN; i++)
  {
    FPGA_Send_Byte(FPGA_BIN_BUF[i]);
  }
  
  // 步骤5：发送完成后，等待DONE拉高
  CDC_Transmit_FS((uint8_t*)"[FPGA] Waiting DONE high...\r\n", 28);
  if(FPGA_Wait_DONE_High() != HAL_OK)
  {
    CDC_Transmit_FS((uint8_t*)"[FPGA] DONE timeout\r\n", 22);
    return HAL_TIMEOUT;
  }
  
  // 步骤6：配置完成，拉高CCLK/DATA0
  HAL_GPIO_WritePin(FPGA_CCLK_PORT, FPGA_CCLK_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(FPGA_DATA0_PORT, FPGA_DATA0_PIN, GPIO_PIN_SET);
  
  CDC_Transmit_FS((uint8_t*)"[FPGA] Config complete\r\n", 24);
  return HAL_OK;
}

void FPGA_Config_Main(void)
{
  // 1. USB CDC接收.bin文件到RAM
  if(FPGA_USB_Recv_Bin_To_RAM() != HAL_OK)
  {
    CDC_Transmit_FS((uint8_t*)"[FPGA] Recv failed\r\n", 20);
    return;
  }
  
  // 2. 发送RAM中的数据配置FPGA
  if(FPGA_Send_Bin_From_RAM() != HAL_OK)
  {
    CDC_Transmit_FS((uint8_t*)"[FPGA] Config failed\r\n", 22);
    return;
  }
  
  // 3. 配置成功
  CDC_Transmit_FS((uint8_t*)"[FPGA] All operations success\r\n", 32);
}

/* FPGA复位函数：拉低PROG_B，复位FPGA */
void FPGA_Reset(void)
{
    g_fpga_state = FPGA_STATE_RESET;
    /* 拉低PROG_B至少1ms（满足XC7A100T要求） */
    HAL_GPIO_WritePin(FPGA_PROGB_PORT, FPGA_PROGB_PIN, GPIO_PIN_RESET);
    HAL_Delay(2);  // 2ms确保复位完成
    HAL_GPIO_WritePin(FPGA_PROGB_PORT, FPGA_PROGB_PIN, GPIO_PIN_SET);
    /* 复位后等待FPGA上电稳定 */
    HAL_Delay(10);
    g_fpga_state = FPGA_STATE_WAIT_INITB;
}

/* USER CODE END 0 */
