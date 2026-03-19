#ifndef __FPGA_CONFIG_DEFAULT_TASK_H
#define __FPGA_CONFIG_DEFAULT_TASK_H

#include "usb_device.h"
#include "cmsis_os.h"
#include "usbd_cdc_if.h"
#include "stdint.h"
#include "string.h"
#include "stdio.h"
#include "inttypes.h"
#include "gpio.h"

/* USB接收配置 */
#define USB_RECV_BUF_SIZE 1024  
extern uint8_t g_usb_recv_buf[USB_RECV_BUF_SIZE];  
extern uint32_t g_usb_recv_len;                    
extern uint8_t g_usb_recv_flag;                    

/* FPGA配置引脚定义（与你的硬件对应） */
#define FPGA_CCLK_PIN     GPIO_PIN_12
#define FPGA_CCLK_PORT    GPIOG
#define FPGA_DATA0_PIN    GPIO_PIN_8
#define FPGA_DATA0_PORT   GPIOI
#define FPGA_PROGB_PIN    GPIO_PIN_3
#define FPGA_PROGB_PORT   GPIOE
#define FPGA_INITB_PIN    GPIO_PIN_14
#define FPGA_INITB_PORT   GPIOC
#define FPGA_DONE_PIN     GPIO_PIN_13
#define FPGA_DONE_PORT    GPIOC

/* 函数声明 */
void FPGAConfigDefaultTask(void const * argument);
void FPGA_Reset(void);                  // FPGA复位函数
HAL_StatusTypeDef FPGA_Send_Config(uint8_t* data, uint32_t len); // 发送配置数据
int8_t USB_CDC_Recv_Callback(uint8_t* buf, uint32_t* len);
#endif
