#include "FPGAConfigDefaultTask.h"
#include "fpga_config.h"
#include "usb_device.h"
#include "cmsis_os.h"
#include "sdram.h"

/* 全局变量 */
uint8_t g_usb_recv_flag = 0;
static uint8_t g_end_cmd_buf[2] = {0};  // 结束指令缓存
static uint8_t g_end_cmd_idx = 0;       // 结束指令缓存索引
static uint8_t g_log_buf[128] = {0};    // 日志缓冲区（任务中异步发送）
static uint32_t g_log_len = 0;          // 日志长度

/* 批量处理数据字节（封装核心逻辑） */
static void SDRAM_Process_Data_Block(uint8_t* buf, uint32_t len)
{
    uint32_t i = 0;
    uint32_t valid_data_len = 0;
    uint8_t* p_valid_data = buf;
    
    for(i = 0; i < len; i++)
    {
        if(g_sdram_recv_state != SDRAM_RECV_DATA)
        {
            break;
        }
        
        // 检测结束指令（0x55+0xAA）
        if(g_end_cmd_idx == 0 && buf[i] == CMD_END_BIN_BYTE1)
        {
            // 先写入之前的有效数据
            if(valid_data_len > 0 && g_sdram_bin_offset + valid_data_len <= SDRAM_TOTAL_SIZE)
            {
                SDRAM_WriteBuffer(p_valid_data, g_sdram_bin_offset, valid_data_len);
                g_sdram_bin_offset += valid_data_len;
                valid_data_len = 0;
                p_valid_data = &buf[i+1];
            }
            g_end_cmd_buf[g_end_cmd_idx++] = buf[i];
        }
        else if(g_end_cmd_idx == 1)
        {
            g_end_cmd_buf[g_end_cmd_idx++] = buf[i];
            // 校验结束指令
            if(g_end_cmd_buf[0] == CMD_END_BIN_BYTE1 && g_end_cmd_buf[1] == CMD_END_BIN_BYTE2)
            {
                // 结束指令校验通过，写入剩余有效数据
                if(valid_data_len > 0 && g_sdram_bin_offset + valid_data_len <= SDRAM_TOTAL_SIZE)
                {
                    SDRAM_WriteBuffer(p_valid_data, g_sdram_bin_offset, valid_data_len);
                    g_sdram_bin_offset += valid_data_len;
                }
                g_sdram_recv_state = SDRAM_RECV_COMPLETE;
                g_end_cmd_idx = 0;
                break;
            }
            else
            {
                // 结束指令无效，合并到有效数据
                p_valid_data = &buf[i-1];
                valid_data_len += 2;
                g_end_cmd_idx = 0;
            }
        }
        else
        {
            // 正常数据，累计有效长度
            valid_data_len++;
        }
    }
    
    // 写入最后一批有效数据
    if(valid_data_len > 0 && g_sdram_recv_state == SDRAM_RECV_DATA)
    {
        if(g_sdram_bin_offset + valid_data_len <= SDRAM_TOTAL_SIZE)
        {
            SDRAM_WriteBuffer(p_valid_data, g_sdram_bin_offset, valid_data_len);
            g_sdram_bin_offset += valid_data_len;
        }
        else
        {
            // SDRAM写满，仅写入剩余空间
            uint32_t remain_len = SDRAM_TOTAL_SIZE - g_sdram_bin_offset;
            if(remain_len > 0)
            {
                SDRAM_WriteBuffer(p_valid_data, g_sdram_bin_offset, remain_len);
                g_sdram_bin_offset += remain_len;
            }
            // 直接发送溢出日志，避免覆盖
            char tmp_log[128] = {0};
            snprintf(tmp_log, sizeof(tmp_log), "[ERROR] SDRAM Full! Stop Recv (Used: %lu/%lu Bytes)\r\n", 
                        g_sdram_bin_offset, SDRAM_TOTAL_SIZE);
            CDC_Transmit_FS((uint8_t*)tmp_log, strlen(tmp_log));
            g_sdram_recv_state = SDRAM_RECV_COMPLETE;
        }
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
    HAL_GPIO_WritePin(FPGA_CCLK_PORT, FPGA_CCLK_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(FPGA_DATA0_PORT, FPGA_DATA0_PIN, GPIO_PIN_RESET);
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
            g_log_len = snprintf((char*)g_log_buf, sizeof(g_log_buf), "[READY] Send 0x11 to start FPGA config\r\n");
            CDC_Transmit_FS(g_log_buf, g_log_len);
            g_log_len = 0;
            
            // 重置状态，等待下一次启动/配置
            g_sdram_recv_state = SDRAM_RECV_IDLE;
            osDelay(10);
        }
        
        // 检测FPGA配置启动指令
        if(g_fpga_config_start == 1)
        {
            CDC_Transmit_FS((uint8_t*)"[FPGA] Start FPGA configuration...\r\n", 36);
            
            // 从SDRAM发送数据配置FPGA
            HAL_StatusTypeDef ret = FPGA_Send_Bin_From_SDRAM(g_sdram_bin_offset);
            if(ret == HAL_OK)
            {
                CDC_Transmit_FS((uint8_t*)"[FPGA] Configuration success!\r\n", 30);
            }
            else
            {
                CDC_Transmit_FS((uint8_t*)"[FPGA] Configuration failed!\r\n", 30);
            }
            
            // 重置配置标志，等待下一次接收
            g_fpga_config_start = 0;
            g_log_len = snprintf((char*)g_log_buf, sizeof(g_log_buf), "[READY] Wait next start cmd (0x5A)\r\n");
        }
        
        osDelay(10);  // 降低CPU占用，避免抢占USB回调
    }
}
