#include "FPGAConfigDefaultTask.h"

/* 全局缓存：Debug模式下可直接watch这些变量查看接收数据 */
uint8_t g_usb_recv_buf[USB_RECV_BUF_SIZE] = {0};  // USB接收数据缓存
uint32_t g_usb_recv_len = 0;                      // 实际接收字节数
uint8_t g_usb_recv_flag = 0;                      // 接收完成标志
FPGA_StateTypeDef  g_fpga_state;
/* USB CDC接收回调函数（需在usbd_cdc_if.c中注册，替代原有CDC_Receive_FS） */
int8_t USB_CDC_Recv_Callback(uint8_t* buf, uint32_t* len)
{
    /* 1. 清空旧数据 */
    memset(g_usb_recv_buf, 0, USB_RECV_BUF_SIZE);
    
    /* 2. 拷贝新接收的数据到全局缓存（防溢出） */
    if(*len > USB_RECV_BUF_SIZE)
    {
        g_usb_recv_len = USB_RECV_BUF_SIZE;
    }
    else
    {
        g_usb_recv_len = *len;
    }
    memcpy(g_usb_recv_buf, buf, g_usb_recv_len);
    
    /* 3. 设置接收完成标志 */
    g_usb_recv_flag = 1;
    
    /* 5. 重新注册接收缓冲区，准备下一次接收 */
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, buf);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    
    return USBD_OK;
}

/* FreeRTOS USB验证任务：初始化USB+循环检测接收数据 */
void FPGAConfigDefaultTask(void const * argument)
{
    /* 1. USB设备初始化（CubeMX生成的CDC初始化） */
    MX_USB_DEVICE_Init();
    osDelay(100);  // 等待USB设备枚举完成（必加：避免初始化未完成就接收数据）
    
    /* 3. 初始化提示：串口调试助手会收到该消息 */
    char init_msg[] = "[USB Ready] Send data to test recv...\r\n";
    CDC_Transmit_FS((uint8_t*)init_msg, strlen(init_msg));
    
    /* 4. 无限循环：检测接收标志+重置状态（FreeRTOS任务主循环） */
    for(;;)
    {
        /* 检测到新数据接收完成 */
        if(g_usb_recv_flag == 1)
        {
            /* Debug关键点：
               1. 在此处打断点，查看g_usb_recv_buf的内容（就是串口调试助手发送的数据）
               2. g_usb_recv_len为实际接收的字节数
            */
            
            // 打印接收日志（可选：通过调试器查看变量更直观）
            char log_msg[64] = {0};
            snprintf(log_msg, sizeof(log_msg), "Debug: Recv data len = %lu\r\n",  (unsigned long)g_usb_recv_len);
            CDC_Transmit_FS((uint8_t*)log_msg, strlen(log_msg));
            
            // 重置接收标志，准备下一次接收
            g_usb_recv_flag = 0;
        }
        
        osDelay(500);  // 任务延时（降低CPU占用，10ms足够）
    }
}