#include "usart_mouse_key.h"
#include "string.h"
#include "usart.h"

MouseKeyPacket_T MouseKeyPacket;
uint8_t send_buf[MOUSE_KEY_PACKET_LEN] = {0};
uint8_t test_wheel = 0;
/**
 * @brief  组装并发送鼠标键盘数据包（硬件偶校验）
 * @param  packet: 待发送的数据包结构体
 * @retval 无
 */
void USART_Send_MouseKey_Packet(MouseKeyPacket_T *packet)
{
    uint16_t idx = 0;

    if(packet == NULL) return;

    // 步骤1：组装包头
    send_buf[idx++] = packet->header;
    // 步骤2：组装鼠标控制位
    send_buf[idx++] = packet->mouse_ctrl;
		// 步骤3：组装wheel
		if(packet->mouse_wheel == 1 || packet->mouse_wheel == 0xff)
		{
			test_wheel++;
		}
		send_buf[idx++] = packet->mouse_wheel;
    // 步骤4：组装X坐标（小端）
    send_buf[idx++] = (uint8_t)(packet->x_coord & 0xFF);
    send_buf[idx++] = (uint8_t)((packet->x_coord >> 8) & 0xFF);
    // 步骤5：组装Y坐标（小端）
    send_buf[idx++] = (uint8_t)(packet->y_coord & 0xFF);
    send_buf[idx++] = (uint8_t)((packet->y_coord >> 8) & 0xFF);
    // 步骤6：组装Mod值
    send_buf[idx++] = packet->key_mod;
    // 步骤7：组装Key1~Key6
    memcpy(&send_buf[idx], packet->key_code, 6);

    // 发送14字节数据，硬件自动添加偶校验位
		HAL_UART_Transmit_DMA(&huart7, send_buf, MOUSE_KEY_PACKET_LEN);
}
