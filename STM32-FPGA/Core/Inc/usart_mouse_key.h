#ifndef __USART_MOUSE_KEY_H
#define __USART_MOUSE_KEY_H

#include "usart.h"
#include "stdint.h"

// 数据包长度定义
#define MOUSE_KEY_PACKET_LEN 14  // 14字节数据（不含奇偶校验）

// 鼠标按键定义（字节1的bit0）
#define MOUSE_LEFT_BTN    0x01  // 左键
#define MOUSE_RIGHT_BTN   0x02  // 右键
#define MOUSE_WHEEL_BTN   0x04  // 滚轮键

// 滚轮方向定义（字节1的bit1）
#define WHEEL_DIR_UP      0x02  // 向上（bit1置1）
#define WHEEL_DIR_DOWN    0x00  // 向下（bit1置0）

// 奇偶校验模式定义
#define PARITY_MODE_ODD   1     // 奇校验
#define PARITY_MODE_EVEN  0     // 偶校验（默认使用）
#define CURRENT_PARITY_MODE PARITY_MODE_EVEN

// 数据包结构体（便于组装数据）
typedef struct 
{
    uint8_t header;          // 字节0：包头 0x55
    uint8_t mouse_ctrl;      // 字节1：鼠标控制位（按键）
		uint8_t mouse_wheel;		 // 字节2：鼠标滚轮位移（1为向上，FF为向下）
    int16_t x_coord;         // 字节2-3：X坐标（有符号16位）
    int16_t y_coord;         // 字节4-5：Y坐标（有符号16位）
    uint8_t key_mod;         // 字节6：键盘Mod值
    uint8_t key_code[6];     // 字节7-12：Key1~Key6
} MouseKeyPacket_T;

extern MouseKeyPacket_T MouseKeyPacket;
// 函数声明
/**
 * @brief  组装并发送鼠标键盘数据包（含奇偶校验位）
 * @param  packet: 待发送的数据包结构体
 * @retval 无
 */
void USART_Send_MouseKey_Packet(MouseKeyPacket_T *packet);

/**
 * @brief  串口发送指定长度的字节数组（底层发送函数）
 * @param  data: 待发送数据指针
 * @param  len:  发送长度
 * @retval 无
 */
void USART_Send_Array(uint8_t *data, uint16_t len);

extern	uint8_t send_buf[MOUSE_KEY_PACKET_LEN];
extern uint8_t test_wheel;
#endif /* __USART_MOUSE_KEY_H */
