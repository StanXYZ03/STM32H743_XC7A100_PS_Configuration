/*
 * File: FPGAConfigDefaultTask.c
 * File Created: Monday, 16th March 2026 2:51:15 pm
 * Author: 赵祥宇
 * -----
 * Last Modified: Tuesday, 24th March 2026 2:08:00 pm
 * Modified By: 赵祥宇
 * -----
 * Copyright (c) 2026 北京革新创展科技有限公司
 */

#include "FPGAConfigDefaultTask.h"
#include "fpga_config.h"
#include "usb_device.h"
#include "cmsis_os.h"
#include "sdram.h"

/* 全局变量 */
uint8_t g_usb_recv_flag = 0;
static uint8_t g_log_buf[128] = {0};    // 日志缓冲区（任务中异步发送）
static uint32_t g_log_len = 0;          // 日志长度
static uint8_t first_call = 1;


/* 批量处理数据字节 */
static void SDRAM_Process_Data_Block(uint8_t* buf, uint32_t len)
{
    static uint8_t prev = 0;
    static uint8_t started = 0;

    if(first_call)
    {
        g_sdram_bin_offset = 0;
        prev = 0;
        started = 0;
        first_call = 0;
    }

    for(uint32_t i = 0; i < len; i++)
    {
        uint8_t curr = buf[i];

        // 结束条件：只有 55 + AA
        if(prev == CMD_END_BIN_BYTE1 && curr == CMD_END_BIN_BYTE2)
        {
            g_sdram_recv_state = SDRAM_RECV_COMPLETE;
            return;
        }

        if(started && g_sdram_recv_state == SDRAM_RECV_DATA)
        {
            if(g_sdram_bin_offset < SDRAM_TOTAL_SIZE)
            {
                SDRAM_WriteBuffer(&prev, g_sdram_bin_offset, 1);
                g_sdram_bin_offset++;
            }
        }

        started = 1;
        prev = curr;
    }
}

/* USB CDC接收回调：仅处理数据，不发送日志（避免阻塞） */
int8_t USB_CDC_Recv_Callback(uint8_t* buf, uint32_t* len)
{
    uint32_t i = 0;
    
    if(*len == 0 || buf == NULL)
    {
        return USBD_OK;
    }    
		
    // 1. 检测FPGA配置启动指令（0x11）
    FPGA_Check_Config_Cmd(buf, *len);
    
    // 2. 逐块解析协议（优先处理启动/结束指令）
    for(i = 0; i < *len; i++)
    {
        if(g_sdram_recv_state == SDRAM_RECV_IDLE)
        {
            // 空闲状态：检测启动指令0x5A
            if(buf[i] == CMD_START_BIN)
            {
                SDRAM_Bin_Cache_Reset();
                g_sdram_recv_state = SDRAM_RECV_DATA;  // 直接进入数据接收状态
                g_log_len = snprintf((char*)g_log_buf, sizeof(g_log_buf), "[READY] Bin File Recv Ready!\r\n");
                i++;  // 跳过启动指令，处理剩余数据
                break;
            }
            else
            {
                // 非启动指令，忽略
                continue;
            }
        }
        else
        {
            break;
        }
    }
    
    // 3. 处理剩余数据（批量写入SDRAM）
    if(i < *len && g_sdram_recv_state == SDRAM_RECV_DATA)
    {
        SDRAM_Process_Data_Block(&buf[i], *len - i);
    }
    
    // 4. 废弃原FPGA缓冲区写入，移除溢出日志
    g_usb_recv_flag = 1;
    return USBD_OK;
}

/* FreeRTOS核心任务：异步处理日志 + 协议状态机 + FPGA配置 */
void FPGAConfigDefaultTask(void const * argument)
{
    // 1. 初始化SDRAM
    MX_FMC_Init();
    SDRAM_Init_Sequence();
    SDRAM_Bin_Cache_Reset();
    
    // 2. 初始化USB
    MX_USB_DEVICE_Init();
    osDelay(200);
    
    // 3. 初始提示（异步发送，避免阻塞）
    g_log_len = snprintf((char*)g_log_buf, sizeof(g_log_buf), "[INIT] USB + SDRAM Ready! Send 0x5A to start recv bin\r\n");
    CDC_Transmit_FS(g_log_buf, g_log_len);
    g_log_len = 0; // 发送后立即重置

    // 4. 初始化FPGA引脚
    HAL_GPIO_WritePin(FPGA_PROGB_PORT, FPGA_PROGB_PIN, GPIO_PIN_SET);
  
    // 5. 主循环：异步处理日志 + 检测接收完成 + FPGA配置
    for(;;)
    {
        // 发送待输出的日志（异步，避免阻塞USB回调）
        if(g_log_len > 0)
        {
            CDC_Transmit_FS(g_log_buf, g_log_len);
            g_log_len = 0;  // 重置日志长度
            osDelay(5);     // 增加短延时，确保日志发送完成
        }
        
        // 检测SDRAM接收完成
        if(g_sdram_recv_state == SDRAM_RECV_COMPLETE)
        {
            // 输出统计结果
            g_log_len = snprintf((char*)g_log_buf, sizeof(g_log_buf),
                                "[INFO] Bin File Recv Complete! Total Size: %.2f MB\r\n",
                                (float)g_sdram_bin_offset / 1024 / 1024);
            // 强制发送统计日志
            CDC_Transmit_FS(g_log_buf, g_log_len);
            g_log_len = 0;
            osDelay(10);
            
            // 输出等待配置提示
            g_log_len = snprintf((char*)g_log_buf, sizeof(g_log_buf), "[READY] Send 0xf00f to start FPGA config\r\n");
            CDC_Transmit_FS(g_log_buf, g_log_len);
            g_log_len = 0;
            
            // 重置状态，等待下一次启动/配置
            g_sdram_recv_state = SDRAM_RECV_IDLE;
						first_call = 1;
						
            osDelay(10);
        }
        
        // 检测FPGA配置启动指令
        if(g_fpga_config_start == 1)
        {
            CDC_Transmit_FS((uint8_t*)"[FPGA] Start FPGA configuration...\r\n", 36);
            
            // 在 g_fpga_config_start == 1 分支中，发送前添加
						if(g_sdram_bin_offset > 35)
						{
								uint8_t* p_sdram = (uint8_t*)SDRAM_BASE_ADDR;
								char check_log[64] = {0};
								uint32_t log_pos = 0;
								log_pos += snprintf(check_log + log_pos, sizeof(check_log) - log_pos, 
																		"[SDRAM Check] 30-35: ");
								for(int i = 30; i <= 35; i++)
								{
										log_pos += snprintf(check_log + log_pos, sizeof(check_log) - log_pos, 
																				"%02X ", p_sdram[i]);
								}
								log_pos += snprintf(check_log + log_pos, sizeof(check_log) - log_pos, "\r\n");
								CDC_Transmit_FS((uint8_t*)check_log, log_pos);
								osDelay(10);
						}
            // 从SDRAM发送数据配置FPGA
            HAL_StatusTypeDef ret = FPGA_Send_Bin_From_SDRAM(g_sdram_bin_offset);
            if(ret == HAL_OK)
            {
                CDC_Transmit_FS((uint8_t*)"[FPGA] Configuration success!\r\n", 30);
								osDelay(10);
            }
            else
            {
                CDC_Transmit_FS((uint8_t*)"[FPGA] Configuration failed!\r\n", 30);
								osDelay(10);
            }
            
            // 重置配置标志，等待下一次接收
            g_fpga_config_start = 0;
            g_log_len = snprintf((char*)g_log_buf, sizeof(g_log_buf), "[READY] Wait next start cmd (0x5A)\r\n");
        }
        osDelay(10);  // 降低CPU占用，避免抢占USB回调
    }
}
