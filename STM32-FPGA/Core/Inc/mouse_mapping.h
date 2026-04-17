#ifndef __MOUSE_MAPPING_H
#define __MOUSE_MAPPING_H

#include "struct_typedef.h"

// ====================== 配置参数（集中管理，方便修改） ======================
#define SCREEN_WIDTH          1920    // 屏幕宽度
#define SCREEN_HEIGHT         1080    // 屏幕高度
#define MOUSE_SENSITIVITY     0.5f    // 鼠标灵敏度（0.5~1.0）
#define MOUSE_OFFSET_THRESHOLD 1      // 有效偏移阈值
#define SCALE_X_COEFF         ((SCREEN_WIDTH * 100) / 256) // X轴缩放系数
#define SCALE_Y_COEFF         ((SCREEN_HEIGHT * 100) / 256)// Y轴缩放系数

// ====================== 全局变量声明（extern，避免重复定义） ======================
extern int16_t curr_mouse_x;          // 鼠标当前X坐标（初始屏幕中心）
extern int16_t curr_mouse_y;          // 鼠标当前Y坐标（初始屏幕中心）
extern uint8_t mouse_data_valid;      // 鼠标数据有效标记（0=无效，1=有效）
extern uint32_t last_mouse_data;      // 上一帧鼠标数据（用于重复检测）

// ====================== 函数声明（规范注释，明确参数/返回值） ======================
/**
 * @brief  鼠标数据转换为屏幕坐标并输出到LCD
 * @param  reg1_data: FPGA读取的32位鼠标数据（格式：button<<24 | X<<16 | Y<<8 | wheel）
 * @retval 无
 */
void Mouse_Data_Convert_To_Screen(uint32_t reg1_data);

/**
 * @brief  STM32读取FPGA的reg1鼠标数据
 * @param  无
 * @retval 无
 */
void STM32_Read_FPGA_Mouse_Data(void);

/**
 * @brief  鼠标数据有效性检测（仅数据变化时置位有效标记）
 * @param  new_reg1_data: 新读取的32位鼠标数据
 * @retval 无
 */
void Set_Mouse_Data_Valid_Check(uint32_t new_reg1_data);

/**
 * @brief  发送鼠标坐标到上位机（预留接口）
 * @param  x: 鼠标X坐标
 * @param  y: 鼠标Y坐标
 * @retval 无
 */
void Send_Mouse_Pos_To_PC(int32_t x, int32_t y);
void Mouse_Pos_LCD_Show(void);
uint8_t wheel_convert(uint32_t *reg1_data);
#endif /* __MOUSE_MAPPING_H */
