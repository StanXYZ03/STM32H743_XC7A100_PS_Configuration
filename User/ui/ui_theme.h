/**
 * 示波器 UI 主题：与 MainTask.c 配色一致，保证全应用视觉统一。
 * 颜色为 0x00RRGGBB（与现有 GUI_SetColor 用法一致）。
 */
#ifndef UI_THEME_H
#define UI_THEME_H

#define UI_DISP_W       800
#define UI_DISP_H       480
/* 与 MainTask DrawHeader 一致：顶/底条分界 y=33，可视标题区至 y=HDR_H-1 */
#define UI_HDR_H        64
/* 与 LCDConf ROTATE_HEADER_H（66）一致：正文区从 y=66 起与示波器 body 旋转带对齐 */
#define UI_ROT_HEADER_H 66
#define UI_CONTENT_TOP  66
/* 与 MainTask PLOT_X、右侧留白及底部 PLOT 留白一致 */
#define UI_MARGIN_X     24
#define UI_MARGIN_XR    4
#define UI_BOTTOM_PAD   9
#define UI_FOOTER_H     24

#define UI_CLR_BG       0x080C10u
#define UI_CLR_HDR_TOP  0x10161Eu
#define UI_CLR_HDR_BOT  0x0C1018u
#define UI_CLR_PANEL    0x0C1018u
#define UI_CLR_ACCENT   0x203858u
#define UI_CLR_GRID_MJ  0x1A2E46u
#define UI_CLR_GRID_MN  0x0D1828u
#define UI_CLR_AXIS     0x2C4A78u
#define UI_CLR_CH1      0xFF3355u
#define UI_CLR_CH2      0x22EE88u
#define UI_CLR_CH1_BADGE 0x380010u
#define UI_CLR_CH2_BADGE 0x003820u
/* 与 dma2d_wave.h CLR_BG_PLOT 一致，卡片区贴近波形区底色 */
#define UI_CLR_BG_PLOT  0x0A1218u
#define UI_CLR_TEXT_HI  0xD0E4F8u
#define UI_CLR_TEXT_MID 0x6888A8u
#define UI_CLR_TEXT_DIM 0x304050u
#define UI_CLR_DIV      0x1A2A40u
#define UI_CLR_RUN_BG   0x143220u
#define UI_CLR_RUN_FG   0x44FF66u
#define UI_CLR_CARD     0x101820u
#define UI_CLR_CARD_BR  0x2A4060u
#define UI_CLR_SEL      0x1A3550u

#endif /* UI_THEME_H */
