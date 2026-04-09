#ifndef UI_SCREENS_H
#define UI_SCREENS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_SCR_WELCOME = 0,
    UI_SCR_MAIN_MENU,
    UI_SCR_SCOPE,
    UI_SCR_CHANNEL,
    UI_SCR_TRIGGER,
    UI_SCR_MEASURE,
    UI_SCR_SYSTEM,
    UI_SCR_FPGA_CONFIG,
    UI_SCR_FPGA_SLAVE_SERIAL,
    UI_SCR_FPGA_JTAG_SRAM,
    UI_SCR_FPGA_JTAG_FLASH,
    UI_SCR_REMOTE_CONTROL,
    UI_SCR_COUNT
} UI_ScreenId_t;

/**
 * 绘制整屏界面。tick_ms 为当前子界面内已停留毫秒数（动画用）。
 * UI_SCR_SCOPE：实际波形由 MainTask 内 MainTask_Scope_* 绘制，此处勿依赖。
 */
void UI_Screen_Draw(UI_ScreenId_t id, uint32_t tick_ms);

#ifdef __cplusplus
}
#endif

#endif /* UI_SCREENS_H */
