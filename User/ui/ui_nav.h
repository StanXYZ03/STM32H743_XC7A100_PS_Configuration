/**
 * 按键/旋钮导航预留接口：后续在 BSP 键盘中调用 UI_Nav_OnKey，
 * 或在周期任务中实现 UI_Nav_Poll() 内扫描按键。
 */
#ifndef UI_NAV_H
#define UI_NAV_H

#include "ui_screens.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_NAV_KEY_UP = 0,
    UI_NAV_KEY_DOWN,
    UI_NAV_KEY_LEFT,
    UI_NAV_KEY_RIGHT,
    UI_NAV_KEY_OK,
    UI_NAV_KEY_BACK,
    UI_NAV_KEY_MENU,
} UI_NavKey_t;

/** 周期调用（如 UI 任务内）；后续在内部调用 bsp_KeyScan 等 */
void UI_Nav_Poll(void);

/** 由底层按键驱动调用；当前为占位，后续接菜单焦点与 UI_Nav_SetScreen */
void UI_Nav_OnKey(UI_NavKey_t key);

/** 逻辑当前界面（接键后切换；自动演示模式下可由外部同步） */
void UI_Nav_SetScreen(UI_ScreenId_t id);
UI_ScreenId_t UI_Nav_GetScreen(void);

/**
 * 主菜单卡片焦点 0..4（UP/DOWN 修改并请求重绘；其它界面可忽略）。
 */
int UI_Nav_GetMenuFocus(void);
int UI_Nav_GetFpgaModeFocus(void);

/** 测量值/参数等变化时调用，下一帧非示波器界面会重绘（与按键效果相同） */
void UI_Nav_MarkDirty(void);

/** MainTask 用：若返回 1 表示应重绘，并清除内部请求标志 */
int UI_Nav_ConsumeRedraw(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_NAV_H */
