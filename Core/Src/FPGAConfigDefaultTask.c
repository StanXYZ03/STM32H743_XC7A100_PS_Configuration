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
            g_log_len = snprintf((char*)g_log_buf, sizeof(g_log_buf), "[ERROR] SDRAM Full! Stop Recv\r\n");
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
    
    // 逐块解析协议（优先处理启动/结束指令）
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
    
    // 处理剩余数据（批量写入SDRAM）
    if(i < *len && g_sdram_recv_state == SDRAM_RECV_DATA)
    {
        SDRAM_Process_Data_Block(&buf[i], *len - i);
    }
    
    // 重置FPGA接收超时计数器
    FPGA_USB_Recv_Data(buf, *len);
    
    return USBD_OK;
}

// 在FPGAConfigDefaultTask.c中新增测试函数
static void SDRAM_Test(void)
{
    // 改为16位测试数据（适配SDRAM 16位位宽）
    uint16_t test_buf[5] = {0x0102, 0x0304, 0x0506, 0x0708, 0x090A};
    uint16_t read_buf[5] = {0};
    uint8_t log_buf[64] = {0};
    uint32_t i = 0;
    
    // 1. 按16位写入SDRAM（匹配硬件位宽）
    uint16_t* p_sdram_16 = (uint16_t*)SDRAM_BASE_ADDR;
    for(i = 0; i < 5; i++)
    {
        p_sdram_16[i] = test_buf[i]; // 16位写入，地址步长2字节
    }
    osDelay(10);
    
    // 2. 按16位读取
    for(i = 0; i < 5; i++)
    {
        read_buf[i] = p_sdram_16[i];
    }
    
    // 3. 打印16位对比结果
    CDC_Transmit_FS((uint8_t*)"16bit SDRAM Test:\r\n", 18);
		osDelay(5);
    for(i = 0; i < 5; i++)
    {
        snprintf((char*)log_buf, sizeof(log_buf), 
                 "HalfWord %d: Write=0x%04X Read=0x%04X\r\n", 
                 i, test_buf[i], read_buf[i]);
        CDC_Transmit_FS(log_buf, strlen((char*)log_buf));
				osDelay(5);
    }
    
    // 4. 最终判断
    uint8_t pass = 1;
    for(i = 0; i < 5; i++)
    {
        if(read_buf[i] != test_buf[i]) pass = 0;
    }
    if(pass)
    {
        CDC_Transmit_FS((uint8_t*)"16bit Test OK! 8bit access error confirmed\r\n", 42);
				osDelay(5);
    }
    else
    {
        CDC_Transmit_FS((uint8_t*)"16bit Test Fail! Check BA0/BA1 pins\r\n", 38);
				osDelay(5);
    }
}

// 在任务主循环前调用测试
void FPGAConfigDefaultTask(void const * argument)
{
    MX_FMC_Init();
    SDRAM_Init_Sequence();
    osDelay(500);
    MX_USB_DEVICE_Init();
    
    for(;;)
    {
			// 先执行SDRAM测试
    SDRAM_Test(); // 关键：先验证SDRAM硬件
		osDelay(500);
		}
    // ... 后续逻辑
}

/* FreeRTOS核心任务：异步处理日志 + 协议状态机 */
//void FPGAConfigDefaultTask(void const * argument)
//{
//    // 1. 初始化SDRAM
//    MX_FMC_Init();
//    SDRAM_Init_Sequence();
//    SDRAM_Bin_Cache_Reset();
//    
//    // 2. 初始化USB
//    MX_USB_DEVICE_Init();
//    osDelay(200);
//    
//    // 3. 初始提示（异步发送，避免阻塞）
//    g_log_len = snprintf((char*)g_log_buf, sizeof(g_log_buf), "[INIT] USB + SDRAM Ready! Send 0x5A to start recv bin\r\n");
//    CDC_Transmit_FS(g_log_buf, g_log_len);

//    // 4. 初始化FPGA引脚
//    HAL_GPIO_WritePin(FPGA_CCLK_PORT, FPGA_CCLK_PIN, GPIO_PIN_RESET);
//    HAL_GPIO_WritePin(FPGA_DATA0_PORT, FPGA_DATA0_PIN, GPIO_PIN_RESET);
//    HAL_GPIO_WritePin(FPGA_PROGB_PORT, FPGA_PROGB_PIN, GPIO_PIN_SET);
//  
//    // 5. 主循环：异步处理日志 + 检测接收完成
//    for(;;)
//    {
//        // 发送待输出的日志（异步，避免阻塞USB回调）
//        if(g_log_len > 0)
//        {
//            CDC_Transmit_FS(g_log_buf, g_log_len);
//            g_log_len = 0;  // 重置日志长度
//        }
//        
//        // 检测接收完成
//        if(g_sdram_recv_state == SDRAM_RECV_COMPLETE)
//        {
//            // 输出统计结果
//            g_log_len = snprintf((char*)g_log_buf, sizeof(g_log_buf),
//                                "[INFO] Bin File Recv Complete! Total Size: %.2f MB\r\n",
//                                (float)g_sdram_bin_offset / 1024 / 1024);
//            // 重置状态，等待下一次启动
//            g_sdram_recv_state = SDRAM_RECV_IDLE;
//            osDelay(10);  // 短暂延时，避免日志刷屏
//            g_log_len = snprintf((char*)g_log_buf, sizeof(g_log_buf), "[READY] Wait next start cmd (0x5A)\r\n");
//        }
//        
//        osDelay(10);  // 降低CPU占用，避免抢占USB回调
//    }
//}
