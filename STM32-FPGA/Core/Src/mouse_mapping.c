#include "mouse_mapping.h"
#include "stdlib.h"   // abs()函数
#include "spi_communication.h"
#include "usart.h"
#include "delay.h"

// 全局变量
int16_t curr_mouse_x = SCREEN_WIDTH / 2;
int16_t curr_mouse_y = SCREEN_HEIGHT / 2;
uint8_t mouse_data_valid = 0;
uint32_t last_mouse_data = 0;

// 滚轮按键处理
uint8_t last_wheel_val = 0x00;    
uint8_t last_ctrl_data = 0;    		
// 新增：标记「是否有真实位移」，彻底阻断无效累加
uint8_t has_real_mouse_move = 0;

// 坐标计算函数（核心修复：仅当有真实位移时才更新坐标）
void Mouse_Data_Convert_To_Screen(uint32_t reg1_data)
{
    uint8_t mouse_x_raw = (reg1_data >> 16) & 0xFF;
    uint8_t mouse_y_raw = (reg1_data >> 8)  & 0xFF;
    int8_t x_offset = (int8_t)mouse_x_raw;
    int8_t y_offset = (int8_t)mouse_y_raw;
    int32_t move_x = 0;
    int32_t move_y = 0;

    // 第一步：强制清零微小偏移（阈值加倍，彻底过滤）
    if(abs(x_offset) < (MOUSE_OFFSET_THRESHOLD * 2)) x_offset = 0;
    if(abs(y_offset) < (MOUSE_OFFSET_THRESHOLD * 2)) y_offset = 0;

    // 第二步：标记「是否有真实位移」
    has_real_mouse_move = (x_offset != 0 || y_offset != 0) ? 1 : 0;
    if(!has_real_mouse_move) {
        return; // 无真实位移，直接返回，不执行任何累加
    }

    y_offset = -y_offset;

    // 计算位移
    move_x = (x_offset * SCALE_X_COEFF * MOUSE_SENSITIVITY) / 100;
    move_y = (y_offset * SCALE_Y_COEFF * MOUSE_SENSITIVITY) / 100;

    // 第三步：位移值二次过滤（避免系数放大导致的微小非零值）
    if(abs(move_x) < 1) move_x = 0;
    if(abs(move_y) < 1) move_y = 0;

    // 仅当位移值非零时才更新坐标
    if(move_x != 0) curr_mouse_x += move_x;
    if(move_y != 0) curr_mouse_y += move_y;

    // 边界保护
    curr_mouse_x = (curr_mouse_x < 0) ? 0 : (curr_mouse_x >= SCREEN_WIDTH ? SCREEN_WIDTH - 1 : curr_mouse_x);
    curr_mouse_y = (curr_mouse_y < 0) ? 0 : (curr_mouse_y >= SCREEN_HEIGHT ? SCREEN_HEIGHT - 1 : curr_mouse_y);
}

uint8_t wheel_convert(uint32_t *reg1_data)
{
    if(reg1_data == NULL) return 0;
    uint8_t current_wheel_val = (*reg1_data) & 0xFF;
    if (current_wheel_val == 0x01 || current_wheel_val == 0xFF) {
        return current_wheel_val;
    }
    return 0;
}

void Set_Mouse_Data_Valid_Check(uint32_t new_reg1_data)
{
    uint8_t x_raw = (new_reg1_data >> 16) & 0xFF;
    uint8_t y_raw = (new_reg1_data >> 8) & 0xFF;
    uint8_t wheel_raw = new_reg1_data & 0xFF;
    uint8_t button_raw = (new_reg1_data >> 24) & 0xFF;

    uint8_t is_wheel_active = (wheel_raw != last_wheel_val);
    uint8_t is_idle = (x_raw == 0) && (y_raw == 0) && (wheel_raw == 0) && (button_raw == 0);
    // 新增：判定「是否有真实位移」
    uint8_t has_move = (x_raw != 0 || y_raw != 0) ? 1 : 0;

    // 核心修复1：仅当「有真实位移/滚轮变化」时，才更新last_mouse_data
    if(!is_idle && (new_reg1_data != last_mouse_data) && (has_move || is_wheel_active))  
    {
        mouse_data_valid = 1;
        last_mouse_data = new_reg1_data;
        last_ctrl_data = button_raw;
        last_wheel_val = wheel_raw;
    }
    // 核心修复2：长按按键时，仅保留按键状态，不触发鼠标数据有效性
    else if( (last_ctrl_data == button_raw) && (button_raw != 0))
    {
        mouse_data_valid = 0; // 关键：长按按键时直接置0，阻断坐标更新
        last_ctrl_data = button_raw;
        // 仅保留按键状态，不更新任何位移/滚轮相关变量
    }
    else if (is_wheel_active) 
    {
        mouse_data_valid = 1;
        last_mouse_data = new_reg1_data;
        last_ctrl_data = button_raw;
        last_wheel_val = wheel_raw;
    }
    else
    {
        mouse_data_valid = 0;
        has_real_mouse_move = 0; // 重置位移标记
    }
}