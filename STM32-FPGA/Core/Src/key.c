/**
 * @file key_mapping.c
 * @author 赵祥宇
 * @brief 键盘HID数据解析函数实现文件（兼容C89，匹配新寄存器定义）
 * @version 1.0
 * @date 2026-03-06
 * 
 * @copyright Copyright (c) 2026 北京革新创展科技有限公司
 * 
 */

#include "key.h"
#include <stdio.h>
#include <string.h>
#include "delay.h"

Key_State_t key_state;
// 标准USB HID键盘码表（核心常用键值映射）
static const struct {
    uint8_t hid_code;
    char    name[16];
} hid_key_map[] = {
    {0x00, "NONE"},      // 无按键
    {0x04, "A"},         // A键
    {0x05, "B"},         // B键
    {0x06, "C"},         // C键
    {0x07, "D"},         // D键
    {0x08, "E"},         // E键
    {0x09, "F"},         // F键
    {0x0A, "G"},         // G键
    {0x0B, "H"},         // H键
    {0x0C, "I"},         // I键
    {0x0D, "J"},         // J键
    {0x0E, "K"},         // K键
    {0x0F, "L"},         // L键
    {0x10, "M"},         // M键
    {0x11, "N"},         // N键
    {0x12, "O"},         // O键
    {0x13, "P"},         // P键
    {0x14, "Q"},         // Q键
    {0x15, "R"},         // R键
    {0x16, "S"},         // S键
    {0x17, "T"},         // T键
    {0x18, "U"},         // U键
    {0x19, "V"},         // V键
    {0x1A, "W"},         // W键
    {0x1B, "X"},         // X键
    {0x1C, "Y"},         // Y键
    {0x1D, "Z"},         // Z键
    {0x1E, "1"},         // 数字1
    {0x1F, "2"},         // 数字2
    {0x20, "3"},         // 数字3
    {0x21, "4"},         // 数字4
    {0x22, "5"},         // 数字5
    {0x23, "6"},         // 数字6
    {0x24, "7"},         // 数字7
    {0x25, "8"},         // 数字8
    {0x26, "9"},         // 数字9
    {0x27, "0"},         // 数字0
    {0x28, "ENTER"},     // 回车键
    {0x29, "ESC"},       // ESC键
    {0x2A, "BACKSPACE"}, // 退格键
    {0x2B, "TAB"},       // TAB键
    {0x2C, "SPACE"},     // 空格键
    {0x2D, "-"},         // 减号
    {0x2E, "="},         // 等号
    {0x2F, "["},         // 左中括号
    {0x30, "]"},         // 右中括号
    {0x31, "\\"},        // 反斜杠
    {0x32, ";"},         // 分号
    {0x33, "'"},         // 单引号
    {0x34, "`"},         // 反引号
    {0x35, ","},         // 逗号
    {0x36, "."},         // 句号
    {0x37, "/"},         // 斜杠
    {0x38, "CAPS_LOCK"}, // 大小写锁定
    {0x39, "F1"},        // F1
    {0x3A, "F2"},        // F2
    {0x3B, "F3"},        // F3
    {0x3C, "F4"},        // F4
    {0x3D, "F5"},        // F5
    {0x3E, "F6"},        // F6
    {0x3F, "F7"},        // F7
    {0x40, "F8"},        // F8
    {0x41, "F9"},        // F9
    {0x42, "F10"},       // F10
    {0x43, "F11"},       // F11
    {0x44, "F12"},       // F12
    {0x00, ""}           // 映射表结束
};

// 修饰键名称映射（基础版）
static const char* modifier_names[] = {
    "NONE",
    "L_CTRL",
    "L_SHIFT",
    "L_CTRL+L_SHIFT",
    "L_ALT",
    "L_CTRL+L_ALT",
    "L_SHIFT+L_ALT",
    "L_CTRL+L_SHIFT+L_ALT",
    "L_WIN",
    "L_CTRL+L_WIN",
    "L_SHIFT+L_WIN",
    "L_CTRL+L_SHIFT+L_WIN",
    "L_ALT+L_WIN",
    "L_CTRL+L_ALT+L_WIN",
    "L_SHIFT+L_ALT+L_WIN",
    "L_CTRL+L_SHIFT+L_ALT+L_WIN",
    "R_CTRL",
    "R_SHIFT"
};

// 键值转名称函数（内部静态函数）
static void hid_code_to_name(uint8_t code, char* name, uint8_t len) {
    int i; 
    for (i = 0; hid_key_map[i].hid_code != 0x00 || hid_key_map[i].name[0] != '\0'; i++) {
        if (hid_key_map[i].hid_code == code) {
            strncpy(name, hid_key_map[i].name, len-1);
            name[len-1] = '\0';
            return;
        }
    }
    strncpy(name, "UNKNOWN", len-1); 
    name[len-1] = '\0';
}

// 核心键盘解析函数（严格匹配新寄存器定义）
int8_t key_mapping(uint32_t reg_data0, uint32_t reg_data1, uint32_t reg_data2, Key_State_t* key_state) {
    // C89：所有变量声明放在代码块开头
    uint8_t valid;
    uint8_t type;
    uint8_t special_key;  // reg_data1[31:24] 特殊按键（修饰键）
    uint8_t K1, K2, K3, K4, K5, K6; // 按你的定义命名
    int i;

    // 1. 参数校验
    if (key_state == NULL) {
        return -1;
    }
    memset(key_state, 0, sizeof(Key_State_t)); 

    // 2. 校验数据有效性和类型
    valid = reg_data0 & 0xFF;          // reg_data0[7:0] = valid
    type  = (reg_data0 >> 8) & 0xFF;   // reg_data0[15:8] = type
    if (valid != 0x01 || type != 0x00) { // 仅处理键盘有效数据
        return -1;
    }

    // 3. 严格按你的寄存器定义解析K1~K6和特殊键
    special_key = (reg_data1 >> 24) & 0xFF; // reg_data1[31:24] = 特殊按键（修饰键）
    // reg_data1[23:16] = 保留0，无需解析
    K1 = (reg_data1 >> 8) & 0xFF;           // reg_data1[15:8] = K1
    K2 = reg_data1 & 0xFF;                  // reg_data1[7:0] = K2
    K3 = (reg_data2 >> 24) & 0xFF;          // reg_data2[31:24] = K3
    K4 = (reg_data2 >> 16) & 0xFF;          // reg_data2[23:16] = K4
    K5 = (reg_data2 >> 8) & 0xFF;           // reg_data2[15:8] = K5
    K6 = reg_data2 & 0xFF;                  // reg_data2[7:0] = K6

    // 4. 填充修饰键（特殊键）
    key_state->modifier = special_key;
    key_state->reserved = 0; // 保留位统一置0

    // 5. 填充K1~K6到key_codes数组（严格对应）
    key_state->key_codes[0] = K1; // K1 → key_codes[0]
    key_state->key_codes[1] = K2; // K2 → key_codes[1]
    key_state->key_codes[2] = K3; // K3 → key_codes[2]
    key_state->key_codes[3] = K4; // K4 → key_codes[3]
    key_state->key_codes[4] = K5; // K5 → key_codes[4]
    key_state->key_codes[5] = K6; // K6 → key_codes[5]

    // 6. 转换按键码为名称，并统计有效按键数
    for (i = 0; i < 6; i++) {
        hid_code_to_name(key_state->key_codes[i], key_state->key_names[i], sizeof(key_state->key_names[i]));
        if (key_state->key_codes[i] != 0x00) { // 0x00表示无按键
            key_state->key_count++;
        }
    }

    return 0; 
}

//// LCD显示函数
//void print_key_state(Key_State_t* key_state) {
//    uint8_t lcd_buf[64] = {0};  
//    uint16_t base_x = lcddev.width/2 - 38*LCD_POINT/4; 
//    uint16_t base_y = lcddev.height/2 - 4*LCD_POINT + 6*LCD_POINT; 
//    char mod_str[32] = "NONE"; 
//    // 参数校验：空指针则显示错误
//    if (key_state == NULL) {
//        sprintf((char*)lcd_buf, "KEY ERR");
//        LCD_ShowString(base_x, base_y, 500, LCD_POINT, LCD_POINT, (char *)lcd_buf);
//        delay_ms(1);
//        return;
//    }

//    // ====================== 第1行：特殊键（最多2个） ======================
//    // 【修改点1】仅清空MOD行区域 - 开始
//    // 填充区域：x起始=base_x，y起始=base_y，x结束=屏幕宽度，y结束=base_y+LCD_POINT
//    LCD_Fill(base_x, base_y, lcddev.width, base_y + LCD_POINT, WHITE); // 背景色替换为实际值
//    // 【修改点1】仅清空MOD行区域 - 结束

//    if (key_state->modifier != 0) {
//        uint8_t mod_count = 0;
//        memset(mod_str, 0, sizeof(mod_str));
//        // 按优先级取最多2个特殊键
//        if (key_state->modifier & 0x02) { // L_SHIFT
//            strcat(mod_str, "L_SHIFT ");
//            mod_count++;
//        }
//        if (mod_count < 2 && (key_state->modifier & 0x01)) { // L_CTRL
//            strcat(mod_str, "L_CTRL ");
//            mod_count++;
//        }
//        // 去除末尾空格
//        if (mod_str[strlen(mod_str)-1] == ' ') {
//            mod_str[strlen(mod_str)-1] = '\0';
//        }
//    }
//    sprintf((char*)lcd_buf, "MOD: %s", mod_str);
//    LCD_ShowString(base_x, base_y, 500, LCD_POINT, LCD_POINT, (char *)lcd_buf);
//    delay_ms(1);

//    // ====================== 第2行：K1/K2/K3 ======================
//    // 【可选】如果需要清空K1-K3行，同理添加：
//    // LCD_Fill(base_x, base_y + 1*LCD_POINT, lcddev.width, base_y + 2*LCD_POINT, WHITE);
//    sprintf((char*)lcd_buf, "K1:%-4s K2:%-4s K3:%-4s",
//            key_state->key_names[0], key_state->key_names[1], key_state->key_names[2]);
//    LCD_ShowString(base_x, base_y + 1*LCD_POINT, 500, LCD_POINT, LCD_POINT, (char *)lcd_buf);
//    delay_ms(1);

//    // ====================== 第3行：K4/K5/K6 ======================
//    // 【可选】如果需要清空K4-K6行，同理添加：
//    // LCD_Fill(base_x, base_y + 2*LCD_POINT, lcddev.width, base_y + 3*LCD_POINT, WHITE);
//    sprintf((char*)lcd_buf, "K4:%-4s K5:%-4s K6:%-4s",
//            key_state->key_names[3], key_state->key_names[4], key_state->key_names[5]);
//    LCD_ShowString(base_x, base_y + 2*LCD_POINT, 500, LCD_POINT, LCD_POINT, (char *)lcd_buf);
//    delay_ms(1);
//}
