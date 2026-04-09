/*
 * 与 LCDConf_Lin_Template.c 中 Header 静态旋转配合：
 * 逻辑 Header 仅在应用侧重绘后调用 LCDConf_RequestHeaderRotate()，
 * 下一帧 ManualRotateToPhysical 才会把 Header 旋到物理缓冲（省 CPU）。
 */
#ifndef LCD_ROTATE_REQUEST_H
#define LCD_ROTATE_REQUEST_H

#ifndef USE_ROTATE_HEADER_STATIC_SKIP
#define USE_ROTATE_HEADER_STATIC_SKIP  1
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** LTDC 行中断未及时到达时 WaitVsync 超时恢复次数（底板噪声时可能偶发递增） */
extern volatile uint32_t g_lcdconf_vsync_timeout_cnt;

void LCDConf_RequestHeaderRotate(void);

/**
 * 下一帧 ManualRotateToPhysical 对整块逻辑缓冲做整屏旋转（与开机前 2 帧相同路径）。
 * 用于全屏 emWin 界面（欢迎/菜单等）：部分旋转 + Body 静态跳过不会每帧更新整屏到物理竖屏。
 */
void LCDConf_RequestFullScreenRotate(void);

#ifdef __cplusplus
}
#endif

#endif /* LCD_ROTATE_REQUEST_H */
