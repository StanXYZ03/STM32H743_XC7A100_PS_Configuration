#ifndef DMA2D_WAVE_H
#define DMA2D_WAVE_H

#include <stdint.h>

/* =========================================================================
 * 【波形区只有边界线/接缝不对】→ 搜本文件：LCD_PLOTBUF_CCW_SEAM_FIX（在 PLOT_BUF 尺寸宏下面）
 *   CCW 时默认已开启；要关掉改为在工程里 #define LCD_PLOTBUF_CCW_SEAM_FIX 0
 *   改宏后须全编 + 复位（会重新执行 DMA2D_PlotBuf_Init）。
 * ========================================================================= */

/*
 * DMA2D 波形位图方案（方案一：PlotBuf 直接竖屏）
 * ─────────────────────────────────────────────────────────────────────
 * 须与 emWin/Config/LCDConf_Lin_Template.c 中 ManualRotate 旋转方向一致。
 *
 * LCD_USE_CCW_ROTATION=0（顺时针 CW）：
 *   PlotBuf: phys_x = PLOT_H-1-amp, phys_y = px
 * LCD_USE_CCW_ROTATION=1（逆时针 CCW + PlotBuf）：
 *   PlotBuf: phys_x = PLOT_H-1-amp；phys_y 默认 PLOT_BUF_H-1-px（时间轴镜像，与网格一致，波形区才正确）。
 *   若仅「与边距接缝/消失区边界线」不对：用 LCD_PLOTBUF_CCW_SEAM_FIX（见文件顶部说明），勿改 MIRROR_TIME_Y 为 0。
 *   MainTask 用 PLOT_CY_CH1/CH2 交换 1/4 与 3/4 基线，使竖屏上 CH1 在上、CH2 在下。
 *   （勿对 Header 做 phys_x=65-src_y 类“镜像”：会交换逻辑行序，标题会跑到状态栏下面）
 * CCW 时 Header 用 Optimize_Rotate_90CCW_RGB565_Header_Custom（见下两项子开关）。
 * 勿对 phys_x 用 65-ly 替换 ly（会上下对调标题与 CH 带）。
 */

/* 1=逆时针90°+部分旋转+PlotBuf；0=顺时针 CW
 *
 * 如何确认当前是 CCW 还是 CW：看本文件下一行宏的值（1=CCW，0=CW），须与
 * emWin/Config/LCDConf_Lin_Template.c 里 ROTATE_FULL_SCREEN / ROTATE_REGION 一致。
 * 改向后 Clean+全量重编、复位，PlotBuf 建议重新 Init。
 *
 * 若 CW/CCW 两种 Header 仍异常，再查 LTDC/面板扫描；勿用 phys_x=常数-src_y 当水平翻转。
 */
#ifndef LCD_USE_CCW_ROTATION
#define LCD_USE_CCW_ROTATION  1
#endif

/*
 * CCW 旋转后整屏在物理竖屏 (480×800) 上的平移（像素）。
 * 按当前 CCW 映射：负值 = 向左、向上；若与肉眼左右相反可把 X 改为 +10。
 * 须与 LCDConf_Lin_Template.c 中 ManualRotate 一致。
 */
#ifndef LCD_CCW_PHYS_OFFSET_X
#define LCD_CCW_PHYS_OFFSET_X  (1)
#endif
#ifndef LCD_CCW_PHYS_OFFSET_Y
#define LCD_CCW_PHYS_OFFSET_Y  (5)
#endif

/* 平移后未写入的露边条带用此色填充（RGB565，近似 MainTask CLR_BG_SCREEN 0x080C10） */
#ifndef LCD_CCW_PHYS_MARGIN_RGB565
#define LCD_CCW_PHYS_MARGIN_RGB565  0x0862u
#endif

/* CCW 时 Header：1=逻辑 X 镜像 + CCW（你要的「横屏左右调换」）；0=与边距同 ROTATE_REGION。
 * 兼容旧工程曾用的 LCD_HEADER_CCW_MIRROR_PHYS_X（与开关语义相同，仅名称曾误导）。 */
#ifndef LCD_HEADER_CCW_MIRROR_LOGICAL_X
#ifdef LCD_HEADER_CCW_MIRROR_PHYS_X
#define LCD_HEADER_CCW_MIRROR_LOGICAL_X  LCD_HEADER_CCW_MIRROR_PHYS_X
#else
#define LCD_HEADER_CCW_MIRROR_LOGICAL_X  1
#endif
#endif

/* 仅当 LCD_HEADER_CCW_MIRROR_LOGICAL_X=1 时生效（否则 Header 走 ROTATE_REGION）：
 * MIRROR_LX：1=读源列左右镜像；0=不镜像。
 * PHYS_Y_MAP_LX：1=phys_y=lx；0=phys_y=799-lx（与 ROTATE_REGION 一致）。
 * 字反/单字镜像时可试：(1,1) 默认 (1,0) 旧行为 (0,1) 仅反条方向 (0,0)=等同 Region。 */
#ifndef LCD_HEADER_CCW_HEADER_MIRROR_LX
#define LCD_HEADER_CCW_HEADER_MIRROR_LX  1
#endif
#ifndef LCD_HEADER_CCW_HEADER_PHYS_Y_MAP_LX
#define LCD_HEADER_CCW_HEADER_PHYS_Y_MAP_LX  1
#endif

/* 0=禁用 DMA2D PlotBuf，改用 GUI 绘制（排查波形时用） */
#define USE_DMA2D_PLOTBUF  1

/* 诊断模式：0=正常 1~4=诊断 */
#define DMA2D_PLOT_BUF_DEBUG  0

/* 波形画线：1=斜线拆成水平/竖直段，DMA2D R2M 填充（默认，省 CPU）；0=逐像素 CPU Bresenham（排障） */
#ifndef USE_DMA2D_WAVEFORM_DRAW
#define USE_DMA2D_WAVEFORM_DRAW  1
#endif

/* 位图尺寸：竖屏 395×772（物理 plot 区，对应逻辑 772×395 旋转后，+10px 向下） */
#define PLOT_BUF_W  395
/* 时间方向高度；CCW 时 LCDConf 中 PHYS_PLOT_Y 须与 ROTATE_REGION 公式对齐（800-PLOT_X-PLOT_W），勿居中 */
#define PLOT_BUF_H  772

/* CCW 时 PlotBuf 时间轴(phys_y)是否对 px 镜像：1=phys_y=PLOT_BUF_H-1-px（默认，与网格一致，波形区正确）
 * 0=phys_y=px 会整块波形时间反向，仅排错时用。须与 Init 中 PLOT_MY_CCW 一致；改后须 DMA2D_PlotBuf_Init。 */
#ifndef LCD_PLOTBUF_CCW_MIRROR_TIME_Y
#define LCD_PLOTBUF_CCW_MIRROR_TIME_Y  1
#endif

/* ---------- LCD_PLOTBUF_CCW_SEAM_FIX：PlotBuf 四边与 MainTask CLR_BORDER_IN 同色 ----------
 * 减轻 CCW 下「只有与边距接缝的那条边界线」错位。实现见 dma2d_wave.c 内 DMA2D_PlotBuf_Init。
 * 未在工程里定义时：CCW→默认 1，CW→默认 0。要关闭可在 uVision 预处理器加 LCD_PLOTBUF_CCW_SEAM_FIX=0 */
#ifndef LCD_PLOTBUF_CCW_SEAM_FIX
#if LCD_USE_CCW_ROTATION
#define LCD_PLOTBUF_CCW_SEAM_FIX  1
#else
#define LCD_PLOTBUF_CCW_SEAM_FIX  0
#endif
#endif

#if !LCD_USE_CCW_ROTATION
#define LCD_PLOTBUF_PHYS_Y_FROM_PX(px)  (px)
#elif LCD_PLOTBUF_CCW_MIRROR_TIME_Y
#define LCD_PLOTBUF_PHYS_Y_FROM_PX(px)  ((PLOT_BUF_H - 1) - (px))
#else
#define LCD_PLOTBUF_PHYS_Y_FROM_PX(px)  (px)
#endif

/* Plot 区网格背景色（0x00RRGGBB），PlotBuf 与 GUI 共用，保证一致 */
#define CLR_BG_PLOT  0x0A1218u

/* 初始化：填充背景色和网格，必须在 GUI_Init 之后调用。成功返回 1，失败返回 0 */
int DMA2D_PlotBuf_Init(void);

/* 是否已成功初始化（分配成功） */
int DMA2D_PlotBuf_IsReady(void);

/* 每帧开始：等待上一帧的异步恢复完成（若有），再画波形 */
void DMA2D_PlotBuf_StartFrame(void);

/* 每帧结束：在 ManualRotate 前调用，异步启动下一帧背景恢复，与 ManualRotate 并行 */
void DMA2D_PlotBuf_EndFrame(void);

/*
 * 从其它全屏页再次进入示波器前调用：等待 DMA2D、清异步标记，并将双缓冲均从 s_bg_buf 同步恢复，
 * 避免「第 2 次循环」时 PlotBuf 与 ping-pong 状态错乱导致波形区背景/网格异常。
 */
void DMA2D_PlotBuf_ResyncOnScopeEnter(void);

/* 在位图中画线段，竖屏坐标 (phys_x, phys_y)：phys_x=0..404, phys_y=0..771 */
void DMA2D_PlotBuf_DrawLine(int x1, int y1, int x2, int y2, uint32_t color_rgb888);

/* 将位图拷贝到当前绘制缓冲（需在 GUI_MULTIBUF_Begin 之后调用） */
void DMA2D_PlotBuf_Flush(void);

/* 获取位图指针，供 GUI_DrawBitmapExp 使用（走 emWin 旋转路径） */
const void * DMA2D_PlotBuf_GetBuffer(void);

#endif /* DMA2D_WAVE_H */
