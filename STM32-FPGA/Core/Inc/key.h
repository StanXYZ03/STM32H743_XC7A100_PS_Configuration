/*
 * File: key.h
 * File Created: Friday, 6th March 2026 3:35:32 pm
 * Author: 赵祥宇
 * -----
 * Last Modified: Friday, 6th March 2026 3:42:36 pm
 * Modified By: 赵祥宇
 * -----
 * Copyright (c) 2026 北京革新创展科技有限公司
 */


#ifndef __KEY_MAPPING_H__
#define __KEY_MAPPING_H__

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 键盘状态结构体（解析结果输出）
 * 
 */
typedef struct {
    uint8_t modifier;          // 修饰键（HID标准：0x01=左Ctrl,0x02=左Shift,0x04=左Alt,0x08=左Win,0x10=右Ctrl,0x20=右Shift,0x40=右Alt,0x80=右Win）
    uint8_t reserved;          // 保留位（固定0）
    uint8_t key_codes[6];      // 6个按键码（key1~key6）
    char    key_names[6][16];  // 按键码对应的按键名称
    uint8_t key_count;         // 有效按下的按键数量
} Key_State_t;

extern Key_State_t key_state;
/**
 * @brief  核心键盘解析函数
 * @note   解析SPI读取的reg_data0/reg_data1/reg_data2，输出键盘按键状态
 * @param  reg_data0: SPI读取的寄存器0（type+valid）
 * @param  reg_data1: SPI读取的寄存器1（key3/key4/key5/key6）
 * @param  reg_data2: SPI读取的寄存器2（Modifier/Reserved/key2/key1）
 * @param  key_state: 输出参数，存储解析后的键盘状态
 * @retval 0: 解析成功（合法键盘数据）; -1: 解析失败（无效/鼠标数据/参数错误）
 */
int8_t key_mapping(uint32_t reg_data0, uint32_t reg_data1, uint32_t reg_data2, Key_State_t* key_state);

/**
 * @brief  打印键盘状态（调试用）
 * @note   格式化输出解析后的键盘状态信息
 * @param  key_state: 键盘状态结构体指针
 * @retval None
 */
void print_key_state(Key_State_t* key_state);

#ifdef __cplusplus
}
#endif

#endif /* __KEY_MAPPING_H__ */
