#include "bsp.h"
#include "FreeRTOS.h"
#include "task.h"
#include "GUI.h"
#include "WM.h"
#include "bsp_lcd_rgb.h"
#include "dma2d_wave.h"
#include "UI_WaveformCtrl.h"
#include "lcd_rotate_request.h"
#include "ui_display_light_path.h"
#include "MainTask_profile.h"
#include "ui_app_config.h"
#include "ui_screens.h"
#include "ui_nav.h"
#include "FPGAConfigDefaultTask.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef UI_MAIN_ENABLE_PRINTF
#define UI_MAIN_ENABLE_PRINTF 0
#endif

#if UI_MAIN_ENABLE_PRINTF
#define UI_MAIN_PRINTF(...) printf(__VA_ARGS__)
#else
#define UI_MAIN_PRINTF(...) ((void)0)
#endif

#define WAVEFORM_BUFFER_SIZE 800
#define CHANNEL_COUNT        2
#define GRID_COL_DIV         10
#define GRID_ROW_DIV         8

/*
 * 垂直：采样值（相对中点）→ 伏特 → 像素；满屏高度对应 GRID_ROW_DIV 格 × V/div。
 * 水平：真机式——屏上总时间 ≈ GRID_COL_DIV×ms/div；一帧缓冲时长 T_buf=n/Fs（UI_SCOPE_EFFECTIVE_FS_HZ）。
 *       T_scr≥T_buf 时整帧铺满全宽；T_scr<T_buf 时居中取一段放大，调 ms/div 可见波形疏密变化。
 * ADC 量程：与 UI_WaveformCtrl adc_to_display(12bit 中点 2048) 一致，可在包含前 #define 覆盖。
 */
#ifndef UI_WAVEFORM_ADC_VREF
#define UI_WAVEFORM_ADC_VREF  3.3f
#endif
#ifndef UI_WAVEFORM_ADC_FULL_SCALE
#define UI_WAVEFORM_ADC_FULL_SCALE  4096.0f
#endif

/* Logical display size */
#define DISP_W  800
#define DISP_H  480

/* RUN 时帧率封顶：默认 25ms ≈ 40fps（可在工程里 #define MAINTASK_FRAME_PERIOD_MS 覆盖） */
#ifndef MAINTASK_FRAME_PERIOD_MS
#define MAINTASK_FRAME_PERIOD_MS  25U
#endif

/* Header strip */
#define HDR_H   64

/* Waveform area — 24px left margin for Y-axis labels */
/* 底部边距 = 4px 正常留白 + 5px 旋转偏移（LCDConf OFFSET=5），
 * 确保网格底边 y=PLOT_Y+PLOT_H-1 落在可见区 y≤474 以内。      */
#define PLOT_X  24
#define PLOT_Y  (HDR_H + 2 + 10)  /* +10px 向下偏移 */
#define PLOT_W  (DISP_W - PLOT_X - 4)
#define PLOT_H  (DISP_H - PLOT_Y - 9)

/* CCW+PlotBuf：幅度 phys_x=PLOT_H-1-amp 使逻辑 1/4、3/4 在竖屏上与 CW 上下对调，交换基线使 CH1 在上、CH2 在下 */
#if LCD_USE_CCW_ROTATION
#define PLOT_CY_CH1  (PLOT_Y + (PLOT_H * 3) / 4)
#define PLOT_CY_CH2  (PLOT_Y + PLOT_H / 4)
#else
#define PLOT_CY_CH1  (PLOT_Y + PLOT_H / 4)
#define PLOT_CY_CH2  (PLOT_Y + (PLOT_H * 3) / 4)
#endif

/* ── color palette (0x00RRGGBB) ─────────────────────────────── */
#define CLR_BG_SCREEN   0x080C10u
#define CLR_BG_HEADER   0x10161Eu
#define CLR_BG_INFO     0x0C1018u
/* CLR_BG_PLOT 见 dma2d_wave.h，PlotBuf 与 GUI 共用 */
#define CLR_GRID_MINOR  0x0D1828u  /* 细分网格线（极淡，不干扰波形） */
#define CLR_GRID_MAJOR  0x1A2E46u  /* 主网格线（与 CLR_BORDER_IN 呼应） */
#define CLR_AXIS        0x2C4A78u  /* 中心轴线（亮蓝，清晰可辨） */
#define CLR_BORDER_OUT  0x0A1828u
#define CLR_BORDER_IN   0x203858u
#define CLR_CH1         0xFF3355u
#define CLR_CH2         0x22EE88u
#define CLR_CH1_BADGE   0x380010u
#define CLR_CH2_BADGE   0x003820u
#define CLR_RUN_BG      0x143220u
#define CLR_RUN_FG      0x44FF66u
#define CLR_STOP_FG     0xFF4466u   /* STOP 状态：红点 + 红字 */
#define CLR_TEXT_HI     0xD0E4F8u
#define CLR_TEXT_MID    0x6888A8u
#define CLR_TEXT_DIM    0x304050u
#define CLR_DIVIDER     0x1A2A40u

static int UI_IsFpgaDynamicScreen(UI_ScreenId_t sid)
{
    return (sid == UI_SCR_FPGA_SLAVE_SERIAL) ||
           (sid == UI_SCR_FPGA_JTAG_SRAM) ||
           (sid == UI_SCR_FPGA_JTAG_FLASH);
}

static int16_t waveformData[CHANNEL_COUNT][WAVEFORM_BUFFER_SIZE];
static int     s_max_x;

extern volatile unsigned long g_emwin_showbuffer_cnt;
extern volatile unsigned long g_emwin_ltdc_irq_cnt;
extern volatile unsigned long g_emwin_confirm_cnt;
/* 手动旋转：将 emWin 逻辑横屏缓冲旋转到物理竖屏缓冲并同步 VSYNC 切换 */
extern void ManualRotateToPhysical(void);

/* ── grid ──────────────────────────────────────────────────── */

/* 仅绘制网格线（不含背景填充、不含边框）
   主循环每帧调用：执行区域严格在 [PLOT_X, PLOT_Y, PLOT_X+PLOT_W-1, PLOT_Y+PLOT_H-1]
   不会触碰 header、边距、边框区域 → header 保持静态无闪烁。       */
static void DrawGridLines(void)
{
    int i;
    const int w = PLOT_W, h = PLOT_H;
    const int cx = w / 2, cy = h / 2;

    GUI_SetColor(CLR_GRID_MINOR);
    for (i = 0; i <= GRID_COL_DIV * 2; i++) {
        int x = PLOT_X + (i * (w - 1)) / (GRID_COL_DIV * 2);
        GUI_DrawVLine(x, PLOT_Y, PLOT_Y + h - 1);
    }
    for (i = 0; i <= GRID_ROW_DIV * 2; i++) {
        int y = PLOT_Y + (i * (h - 1)) / (GRID_ROW_DIV * 2);
        GUI_DrawHLine(y, PLOT_X, PLOT_X + w - 1);
    }

    GUI_SetColor(CLR_GRID_MAJOR);
    for (i = 0; i <= GRID_COL_DIV; i++) {
        int x = PLOT_X + (i * (w - 1)) / GRID_COL_DIV;
        GUI_DrawVLine(x, PLOT_Y, PLOT_Y + h - 1);
    }
    for (i = 0; i <= GRID_ROW_DIV; i++) {
        int y = PLOT_Y + (i * (h - 1)) / GRID_ROW_DIV;
        GUI_DrawHLine(y, PLOT_X, PLOT_X + w - 1);
    }

    GUI_SetColor(CLR_AXIS);
    GUI_DrawHLine(PLOT_Y + cy, PLOT_X, PLOT_X + w - 1);
    GUI_DrawVLine(PLOT_X + cx, PLOT_Y, PLOT_Y + h - 1);
}

/* 完整 plot 区域初始化绘制（背景 + 网格线 + 双层边框）
   仅在初始化阶段向每个缓冲调用一次；主循环不调用此函数。        */
static void DrawGridFull(void)
{
    GUI_SetColor(CLR_BG_PLOT);
    GUI_FillRect(PLOT_X, PLOT_Y, PLOT_X + PLOT_W - 1, PLOT_Y + PLOT_H - 1);

    DrawGridLines();

    /* 双层边框：位于 plot 区域外侧，FillRect/DrawGridLines 均不覆盖，
       初始化后永久保留，无需每帧重绘。                              */
    GUI_SetColor(CLR_BORDER_OUT);
    GUI_DrawRect(PLOT_X - 2, PLOT_Y - 2, PLOT_X + PLOT_W + 1, PLOT_Y + PLOT_H + 1);
    GUI_SetColor(CLR_BORDER_IN);
    GUI_DrawRect(PLOT_X - 1, PLOT_Y - 1, PLOT_X + PLOT_W,     PLOT_Y + PLOT_H    );
}

/* ── waveforms ─────────────────────────────────────────────── */
static int SampleToY(int sample, int centerY)
{
    float       vdiv = UI_Waveform_GetVoltPerDiv();
    float       volts;
    float       px_per_volt;

    if (vdiv <= 0.0f) {
        vdiv = 1.0f;
    }
    volts = (float)sample * (UI_WAVEFORM_ADC_VREF / UI_WAVEFORM_ADC_FULL_SCALE);
    /* 整幅高度 PLOT_H 对应 GRID_ROW_DIV 格 × V/div */
    px_per_volt = (float)(PLOT_H - 1) / ((float)GRID_ROW_DIV * vdiv);
    {
        int y = centerY - (int)(0.5f + volts * px_per_volt);
        if (y < PLOT_Y) {
            y = PLOT_Y;
        }
        if (y > PLOT_Y + PLOT_H - 1) {
            y = PLOT_Y + PLOT_H - 1;
        }
        return y;
    }
}

/**
 * 将采样序号映射到波形区水平坐标 [0, PLOT_W-1]（PlotBuf 路径不加 PLOT_X）。
 * n_samples 为当前帧有效点数 s_max_x。ms/div 与标称 Fs 共同决定「时间窗」相对整帧的缩放。
 */
static int PlotSampleIndexToLocalX(int i, int n_samples)
{
    int   w_plot = PLOT_W - 1;
    float ms, t_buf_ms, t_scr_ms, frac0, frac1, t_n, span, x_loc;
    int   xi;

    if (n_samples < 2 || w_plot < 1) {
        return 0;
    }
    if (i < 0) {
        i = 0;
    }
    if (i > n_samples - 1) {
        i = n_samples - 1;
    }

    ms = UI_Waveform_GetTimePerDiv();
    if (ms <= 0.0f) {
        ms = 1.0f;
    }
    t_buf_ms = (float)n_samples * (1000.0f / UI_SCOPE_EFFECTIVE_FS_HZ);
    t_scr_ms = (float)GRID_COL_DIV * ms;

    if (t_scr_ms >= t_buf_ms) {
        frac0 = 0.0f;
        frac1 = 1.0f;
    } else {
        float half = 0.5f * (t_scr_ms / t_buf_ms);
        frac0      = 0.5f - half;
        frac1      = 0.5f + half;
    }

    t_n  = (float)i / (float)(n_samples - 1);
    span = frac1 - frac0;
    if (span < 1e-6f) {
        span = 1e-6f;
    }
    x_loc = (t_n - frac0) / span * (float)w_plot;
    xi    = (int)(0.5f + x_loc);
    if (xi < 0) {
        xi = 0;
    }
    if (xi > w_plot) {
        xi = w_plot;
    }
    return xi;
}

/* 单独显示时通道居中；双通道时用原 1/4、3/4 基线并叠加位移 */
static void ScopePickCenters(int *cy1, int *cy2, int *show1, int *show2)
{
    UI_Waveform_Solo_t s = UI_Waveform_GetSolo();
    const int          mid = PLOT_Y + PLOT_H / 2;

    *show1 = (s != UI_WAVEFORM_SOLO_CH2) ? 1 : 0;
    *show2 = (s != UI_WAVEFORM_SOLO_CH1) ? 1 : 0;
    if (s == UI_WAVEFORM_SOLO_CH1) {
        *cy1 = mid + UI_Waveform_GetCH1Offset();
        *cy2 = PLOT_CY_CH2;
    } else if (s == UI_WAVEFORM_SOLO_CH2) {
        *cy1 = PLOT_CY_CH1;
        *cy2 = mid + UI_Waveform_GetCH2Offset();
    } else {
        *cy1 = PLOT_CY_CH1 + UI_Waveform_GetCH1Offset();
        *cy2 = PLOT_CY_CH2 + UI_Waveform_GetCH2Offset();
    }
}

/* 波形跳点：1=每点都画，2=每2点画一线（线段数减半），3=每3点… 降低 CPU */
#define WAVEFORM_STRIDE  2

/* 画到位图缓冲，竖屏坐标映射：逻辑(sample,amplitude)→物理(404-amp,sample) */
static void DrawWaveformsToBuf(void)
{
    int x, y1, y2, y1p, y2p;
    int cy1, cy2, show1, show2;

    if (s_max_x < 2) return;
    ScopePickCenters(&cy1, &cy2, &show1, &show2);

    if (show1 != 0) {
        y1p = SampleToY(waveformData[0][0], cy1);
    }
    if (show2 != 0) {
        y2p = SampleToY(waveformData[1][0], cy2);
    }

    for (x = WAVEFORM_STRIDE; x < s_max_x; x += WAVEFORM_STRIDE) {
        int px_prev = PlotSampleIndexToLocalX(x - WAVEFORM_STRIDE, s_max_x);
        int px_curr = PlotSampleIndexToLocalX(x, s_max_x);
        int py_prev = LCD_PLOTBUF_PHYS_Y_FROM_PX(px_prev);
        int py_curr = LCD_PLOTBUF_PHYS_Y_FROM_PX(px_curr);

        if (show1 != 0) {
            y1 = SampleToY(waveformData[0][x], cy1);
        }
        if (show2 != 0) {
            y2 = SampleToY(waveformData[1][x], cy2);
        }

        if (show1 != 0) {
            int amp1p = y1p - PLOT_Y, amp1 = y1 - PLOT_Y;
            int phys_x1p = PLOT_H - 1 - amp1p, phys_x1 = PLOT_H - 1 - amp1;
            DMA2D_PlotBuf_DrawLine(phys_x1p, py_prev, phys_x1, py_curr, CLR_CH1);
            y1p = y1;
        }
        if (show2 != 0) {
            int amp2p = y2p - PLOT_Y, amp2 = y2 - PLOT_Y;
            int phys_x2p = PLOT_H - 1 - amp2p, phys_x2 = PLOT_H - 1 - amp2;
            DMA2D_PlotBuf_DrawLine(phys_x2p, py_prev, phys_x2, py_curr, CLR_CH2);
            y2p = y2;
        }
    }
}

static void DrawWaveforms(void)
{
    int x, y, yp;
    int cy1, cy2, show1, show2;

    if (s_max_x < 2) return;
    ScopePickCenters(&cy1, &cy2, &show1, &show2);

    /* ── CH1 ───────────────────────────────────────────────── */
    if (show1 != 0) {
        GUI_SetColor(CLR_CH1);
        yp = SampleToY(waveformData[0][0], cy1);
        for (x = 1; x < s_max_x; x++) {
            int px_prev = PLOT_X + PlotSampleIndexToLocalX(x - 1, s_max_x);
            int px_curr = PLOT_X + PlotSampleIndexToLocalX(x, s_max_x);
            y = SampleToY(waveformData[0][x], cy1);
            GUI_DrawLine(px_prev, yp, px_curr, y);
            yp = y;
        }
    }

    /* ── CH2 ───────────────────────────────────────────────── */
    if (show2 != 0) {
        GUI_SetColor(CLR_CH2);
        yp = SampleToY(waveformData[1][0], cy2);
        for (x = 1; x < s_max_x; x++) {
            int px_prev = PLOT_X + PlotSampleIndexToLocalX(x - 1, s_max_x);
            int px_curr = PLOT_X + PlotSampleIndexToLocalX(x, s_max_x);
            y = SampleToY(waveformData[1][x], cy2);
            GUI_DrawLine(px_prev, yp, px_curr, y);
            yp = y;
        }
    }

    GUI_SetTextMode(GUI_TM_TRANS);
    GUI_SetFont(&GUI_Font8_ASCII);
    if (show1 != 0) {
        GUI_SetColor(CLR_CH1);
        GUI_DispStringAt("CH1", PLOT_X + PLOT_W - 26, cy1 - 5);
    }
    if (show2 != 0) {
        GUI_SetColor(CLR_CH2);
        GUI_DispStringAt("CH2", PLOT_X + PLOT_W - 26, cy2 - 5);
    }
}

/* ── header ────────────────────────────────────────────────── */
static void DrawHeader(void)
{
    GUI_SetTextMode(GUI_TM_TRANS);

    GUI_SetColor(CLR_BG_HEADER);
    GUI_FillRect(0, 0, DISP_W - 1, 33);
    GUI_SetColor(CLR_BG_INFO);
    GUI_FillRect(0, 34, DISP_W - 1, HDR_H - 1);

    GUI_SetColor(CLR_DIVIDER);
    GUI_DrawHLine(33, 0, DISP_W - 1);
    GUI_DrawHLine(HDR_H, 0, DISP_W - 1);

    GUI_SetFont(&GUI_Font16_ASCII);
    GUI_SetColor(CLR_TEXT_HI);
    {
        const char *title = "DUAL CHANNEL OSCILLOSCOPE";
        int tw = GUI_GetStringDistX(title);
        GUI_DispStringAt(title, DISP_W / 2 - tw / 2, 9);
    }

    GUI_SetColor(CLR_RUN_BG);
    GUI_FillRoundedRect(DISP_W - 72, 11, DISP_W - 8, 30, 4);
    GUI_SetColor(CLR_BORDER_IN);
    GUI_DrawRoundedRect(DISP_W - 72, 11, DISP_W - 8, 30, 4);
    GUI_SetColor(CLR_RUN_FG);
    GUI_FillCircle(DISP_W - 60, 21, 4);
    GUI_SetFont(&GUI_Font13_ASCII);
    GUI_SetColor(CLR_RUN_FG);
    GUI_DispStringAt("RUN", DISP_W - 52, 15);

    GUI_SetColor(CLR_CH1_BADGE);
    GUI_FillRoundedRect(8, 38, 44, 57, 4);
    GUI_SetColor(CLR_CH1);
    GUI_DrawRoundedRect(8, 38, 44, 57, 4);
    GUI_SetFont(&GUI_Font13_ASCII);
    GUI_SetColor(CLR_CH1);
    GUI_DispStringAt("CH1", 14, 42);

    GUI_SetFont(&GUI_Font8_ASCII);
    GUI_SetColor(CLR_TEXT_MID);
    {
        char scale_str[24];
        UI_Waveform_FormatScaleStr(scale_str, sizeof(scale_str));
        GUI_DispStringAt(scale_str, 52, 43);
    }

    GUI_SetColor(CLR_DIVIDER);
    GUI_DrawVLine(220, 36, HDR_H - 3);

    GUI_SetColor(CLR_CH2_BADGE);
    GUI_FillRoundedRect(232, 38, 268, 57, 4);
    GUI_SetColor(CLR_CH2);
    GUI_DrawRoundedRect(232, 38, 268, 57, 4);
    GUI_SetFont(&GUI_Font13_ASCII);
    GUI_SetColor(CLR_CH2);
    GUI_DispStringAt("CH2", 238, 42);

    GUI_SetFont(&GUI_Font8_ASCII);
    GUI_SetColor(CLR_TEXT_MID);
    {
        char scale_str[24];
        UI_Waveform_FormatScaleStr(scale_str, sizeof(scale_str));
        GUI_DispStringAt(scale_str, 276, 43);
    }

    GUI_SetColor(CLR_DIVIDER);
    GUI_DrawVLine(440, 36, HDR_H - 3);

    GUI_SetColor(CLR_TEXT_DIM);
    GUI_DispStringAt("STM32H743  @400MHz", DISP_W - 160, 43);
}

/* ── data update ───────────────────────────────────────────── */
/*
 * 从 ADC 队列取出最新帧，更新显示缓冲和有效点数。
 * 若队列空（ADC 尚未发送新帧），保持旧数据不变（冻结上一帧）。
 * 若波形处于 STOP 状态，UI_Waveform_FetchFrame 内部直接跳过。
 */
static void UpdateWaveformData(void)
{
    int n = UI_Waveform_FetchFrame(waveformData[0], waveformData[1],
                                   WAVEFORM_BUFFER_SIZE);
    if (n > 0) {
        s_max_x = n;   /* 更新有效点数（通常 = ADC_FRAME_SIZE = 600） */
    }
}

/* ── 静态区域（左侧 Y 轴刻度线）────────────────────────────── */
static void DrawYAxisTicks(void)
{
    int j;
    GUI_SetColor(CLR_BORDER_IN);
    for (j = 0; j <= GRID_ROW_DIV; j++) {
        int y = PLOT_Y + (j * (PLOT_H - 1)) / GRID_ROW_DIV;
        GUI_DrawHLine(y, PLOT_X - 5, PLOT_X - 1);
    }
}

/* Header 状态条：静态旋转时仅 RUN/STOP、模式、档位、PlotBuf 标记变化才重绘并请求旋到物理屏 */
#if USE_ROTATE_HEADER_STATIC_SKIP
static void MainTask_UpdateHeaderStatusBar(void)
{
    static uint8_t s_hdr_tracked;
    static int s_prev_run;
    static UI_Waveform_DisplayMode_t s_prev_mode;
    static int s_prev_gui;
    static char s_prev_scale[24];
    static char s_prev_solo[8];

    char scale[24];
    char mode_solo[20];
    int run  = UI_Waveform_IsRunning() ? 1 : 0;
    UI_Waveform_DisplayMode_t mode = UI_Waveform_GetDisplayMode();
    int gui  = DMA2D_PlotBuf_IsReady() ? 1 : 0;
    const char *solo_lbl = UI_Waveform_GetSoloLabel();

    UI_Waveform_FormatScaleStr(scale, sizeof(scale));
    (void)snprintf(mode_solo, sizeof(mode_solo), "%s %s",
                     mode == UI_WAVEFORM_MODE_ROLL ? "Roll" : "Y-T", solo_lbl);

    if (s_hdr_tracked) {
        if (run == s_prev_run && mode == s_prev_mode && gui == s_prev_gui &&
            strcmp(scale, s_prev_scale) == 0 && strcmp(solo_lbl, s_prev_solo) == 0) {
            return;
        }
    }
    s_hdr_tracked = 1u;
    s_prev_run      = run;
    s_prev_mode     = mode;
    s_prev_gui      = gui;
    strncpy(s_prev_scale, scale, sizeof(s_prev_scale) - 1u);
    s_prev_scale[sizeof(s_prev_scale) - 1u] = '\0';
    strncpy(s_prev_solo, solo_lbl, sizeof(s_prev_solo) - 1u);
    s_prev_solo[sizeof(s_prev_solo) - 1u] = '\0';

    GUI_SetColor(CLR_BG_INFO);
    GUI_FillRect(448, 43, 520, 55);
    GUI_SetColor(CLR_TEXT_MID);
    GUI_SetFont(&GUI_Font8_ASCII);
    GUI_DispStringAt(mode_solo, 448, 43);
    if (!DMA2D_PlotBuf_IsReady()) {
        GUI_SetColor(GUI_MAKE_COLOR(0xFF6600));
        GUI_DispStringAt("[GUI]", 488, 43);
    }

    GUI_SetColor(CLR_RUN_BG);
    GUI_FillRoundedRect(DISP_W - 72, 11, DISP_W - 8, 30, 4);
    GUI_SetColor(CLR_BORDER_IN);
    GUI_DrawRoundedRect(DISP_W - 72, 11, DISP_W - 8, 30, 4);
    GUI_SetColor(UI_Waveform_IsRunning() ? CLR_RUN_FG : CLR_STOP_FG);
    GUI_FillCircle(DISP_W - 60, 21, 4);
    GUI_SetFont(&GUI_Font13_ASCII);
    GUI_DispStringAt(UI_Waveform_IsRunning() ? "RUN" : "STOP", DISP_W - 52, 15);

    GUI_SetColor(CLR_BG_INFO);
    GUI_FillRect(52, 43, 218, 55);
    GUI_SetColor(CLR_TEXT_MID);
    GUI_DispStringAt(scale, 52, 43);
    GUI_SetColor(CLR_BG_INFO);
    GUI_FillRect(276, 43, 438, 55);
    GUI_SetColor(CLR_TEXT_MID);
    GUI_DispStringAt(scale, 276, 43);

    LCDConf_RequestHeaderRotate();
}
#endif

/* ── 示波器整页布局（从其它演示页切回波形页时须重画）──────────────── */
static void MainTask_Scope_RestoreLayout(void)
{
    GUI_SetColor(CLR_BG_SCREEN);
    GUI_FillRect(0, 0, DISP_W - 1, DISP_H - 1);
    DrawHeader();
    if (PLOT_Y > HDR_H + 1) {
        GUI_SetColor(CLR_BG_SCREEN);
        GUI_FillRect(0, HDR_H + 1, DISP_W - 1, PLOT_Y - 1);
    }
    GUI_SetColor(CLR_BG_PLOT);
    GUI_FillRect(0, PLOT_Y, PLOT_X - 1, PLOT_Y + PLOT_H - 1);
    if (PLOT_X + PLOT_W < DISP_W)
        GUI_FillRect(PLOT_X + PLOT_W, PLOT_Y, DISP_W - 1, PLOT_Y + PLOT_H - 1);
    if (PLOT_Y + PLOT_H < DISP_H)
        GUI_FillRect(0, PLOT_Y + PLOT_H, DISP_W - 1, DISP_H - 1);
    DrawYAxisTicks();
    DrawGridFull();
    ManualRotateToPhysical();
}

/* ── 示波器单帧渲染（与原先 MainTask while(1) 内主体一致）────────── */
static void MainTask_Scope_RenderFrame(void)
{
    mtp_mark_start();
    UpdateWaveformData();
    mtp_acc(0);

    GUI_MULTIBUF_Begin();
    GUI_SetTextMode(GUI_TM_TRANS);

#if USE_ROTATE_HEADER_STATIC_SKIP
    MainTask_UpdateHeaderStatusBar();
#else
    {
        char mode_solo_nl[20];
        (void)snprintf(mode_solo_nl, sizeof(mode_solo_nl), "%s %s",
                       UI_Waveform_GetDisplayMode() == UI_WAVEFORM_MODE_ROLL ? "Roll" : "Y-T",
                       UI_Waveform_GetSoloLabel());
        GUI_SetColor(CLR_BG_INFO);
        GUI_FillRect(448, 43, 520, 55);
        GUI_SetColor(CLR_TEXT_MID);
        GUI_SetFont(&GUI_Font8_ASCII);
        GUI_DispStringAt(mode_solo_nl, 448, 43);
    }
    if (!DMA2D_PlotBuf_IsReady()) {
        GUI_SetColor(GUI_MAKE_COLOR(0xFF6600));
        GUI_DispStringAt("[GUI]", 488, 43);
    }
    {
        GUI_SetColor(CLR_RUN_BG);
        GUI_FillRoundedRect(DISP_W - 72, 11, DISP_W - 8, 30, 4);
        GUI_SetColor(CLR_BORDER_IN);
        GUI_DrawRoundedRect(DISP_W - 72, 11, DISP_W - 8, 30, 4);
        GUI_SetColor(UI_Waveform_IsRunning() ? CLR_RUN_FG : CLR_STOP_FG);
        GUI_FillCircle(DISP_W - 60, 21, 4);
        GUI_SetFont(&GUI_Font13_ASCII);
        GUI_DispStringAt(UI_Waveform_IsRunning() ? "RUN" : "STOP", DISP_W - 52, 15);
    }
    {
        char scale_str[24];
        UI_Waveform_FormatScaleStr(scale_str, sizeof(scale_str));
        GUI_SetColor(CLR_BG_INFO);
        GUI_FillRect(52, 43, 218, 55);
        GUI_SetColor(CLR_TEXT_MID);
        GUI_DispStringAt(scale_str, 52, 43);
        GUI_SetColor(CLR_BG_INFO);
        GUI_FillRect(276, 43, 438, 55);
        GUI_SetColor(CLR_TEXT_MID);
        GUI_DispStringAt(scale_str, 276, 43);
    }
#endif
    mtp_acc(1);

    if (DMA2D_PlotBuf_IsReady()) {
        DMA2D_PlotBuf_StartFrame();
    }
    mtp_acc(2);

    if (DMA2D_PlotBuf_IsReady()) {
        DrawWaveformsToBuf();
    } else {
        GUI_SetColor(CLR_BG_PLOT);
        GUI_FillRect(PLOT_X, PLOT_Y, PLOT_X + PLOT_W - 1, PLOT_Y + PLOT_H - 1);
        DrawGridLines();
        DrawWaveforms();
    }
    mtp_acc(3);

    if (DMA2D_PlotBuf_IsReady()) {
        GUI_SetColor(CLR_BG_PLOT);
        GUI_FillRect(PLOT_X - 2, PLOT_Y - 2, PLOT_X + PLOT_W + 1, PLOT_Y + PLOT_H + 1);
        DrawYAxisTicks();
    } else {
        GUI_SetColor(CLR_CH1);
        GUI_SetFont(&GUI_Font8_ASCII);
        GUI_DispStringAt("CH1", PLOT_X + PLOT_W - 26, PLOT_CY_CH1 - 5);
        GUI_SetColor(CLR_CH2);
        GUI_DispStringAt("CH2", PLOT_X + PLOT_W - 26, PLOT_CY_CH2 - 5);
        GUI_SetColor(GUI_MAKE_COLOR(0xFF6600));
        GUI_DispStringAt("GUI", PLOT_X + 4, PLOT_Y + PLOT_H - 12);
    }
    mtp_acc(4);

    GUI_MULTIBUF_End();

    if (DMA2D_PlotBuf_IsReady())
        DMA2D_PlotBuf_EndFrame();
    mtp_acc(5);

    ManualRotateToPhysical();
    mtp_acc(6);

    mtp_tick_end_of_frame();
}

/* ── MainTask ──────────────────────────────────────────────── */
void MainTask(void)
{
    /* vTaskStartScheduler 之后 main 不再打印；此处确认 emWin 任务已调度 */
    UI_MAIN_PRINTF("[APP] MainTask started\r\n");

    __HAL_RCC_CRC_CLK_ENABLE();
    GUI_Init();
    UI_MAIN_PRINTF("[APP] GUI_Init OK\r\n");
    GUI_SetTextMode(GUI_TM_TRANS);

#if UI_APP_DEMO_CYCLE_SCREENS
    {
        TickType_t t_screen_start   = xTaskGetTickCount();
        UI_ScreenId_t sid           = UI_SCR_WELCOME;
        TickType_t frame_wake_scope = xTaskGetTickCount();
        UI_ScreenId_t prev_sid     = UI_SCR_SYSTEM; /* != SCOPE：首帧从欢迎页开始不触发波形布局 */
        uint8_t plotbuf_attempted   = 0u;

        UI_Waveform_QueueInit();
        s_max_x = (ADC_FRAME_SIZE < (uint32_t)PLOT_W) ? (int)ADC_FRAME_SIZE : PLOT_W;

        /* 与 sid 初值一致；循环内不再每帧 UI_Nav_SetScreen(sid)，否则会覆盖编码器在 UI_Nav_Poll 里更新的界面 */
        UI_Nav_SetScreen(UI_SCR_WELCOME);

        for (;;) {
            TickType_t now = xTaskGetTickCount();
#if UI_DEMO_AUTO_ADVANCE
            if ((now - t_screen_start) >= pdMS_TO_TICKS(UI_DEMO_SCREEN_MS)) {
                t_screen_start = now;
                sid = (UI_ScreenId_t)(((int)sid + 1) % (int)UI_SCR_COUNT);
                UI_Nav_SetScreen(sid);
            }
#endif

            UI_Nav_Poll();
            sid = UI_Nav_GetScreen();

            UI_Waveform_SetStreamActive(sid == UI_SCR_SCOPE ? 1 : 0);

            if (sid == UI_SCR_SCOPE) {
                if (prev_sid != UI_SCR_SCOPE) {
                    /* 再次进入示波器：PlotBuf 双缓冲/异步恢复与菜单页 DMA2D 交错后易失步，先同步再强制整屏旋一次 body */
                    if (plotbuf_attempted != 0u && DMA2D_PlotBuf_IsReady()) {
                        DMA2D_PlotBuf_ResyncOnScopeEnter();
                        LCDConf_RequestFullScreenRotate();
                    }
                    MainTask_Scope_RestoreLayout();
                    frame_wake_scope = xTaskGetTickCount();
                    if (!plotbuf_attempted) {
                        plotbuf_attempted = 1u;
                        if (!DMA2D_PlotBuf_Init()) {
#if !UI_LIGHT_PATH_RELAX
                            Error_Handler(__FILE__, __LINE__);
#endif
                        }
#if MAINTASK_PROFILE_EMWIN_SEGMENTS
                        UI_MAIN_PRINTF("\r\n[MTP] MainTask profile ON, print every %u frames, see [6a]/[6b]\r\n",
                               (unsigned)MAINTASK_PROFILE_ACCUM_FRAMES);
#endif
                    }
                }
                if (!UI_Waveform_IsRunning()) {
                    vTaskDelay(pdMS_TO_TICKS(50));
                    frame_wake_scope = xTaskGetTickCount();
                }
                MainTask_Scope_RenderFrame();
                if (UI_Waveform_IsRunning()) {
                    vTaskDelayUntil(&frame_wake_scope, pdMS_TO_TICKS(MAINTASK_FRAME_PERIOD_MS));
                }
            } else {
                int need_redraw = (prev_sid != sid) ? 1 : 0;
                if (UI_IsFpgaDynamicScreen(sid)) {
                    need_redraw = 1;
                }
                if (UI_Nav_ConsumeRedraw()) {
                    need_redraw = 1;
                }
                if (need_redraw != 0) {
                    uint32_t tick_ms = (uint32_t)((now - t_screen_start) * portTICK_PERIOD_MS);
                    GUI_MULTIBUF_Begin();
                    GUI_SetTextMode(GUI_TM_TRANS);
                    UI_Screen_Draw(sid, tick_ms);
                    GUI_MULTIBUF_End();
                    LCDConf_RequestFullScreenRotate();
                    ManualRotateToPhysical();
                }
                vTaskDelay(pdMS_TO_TICKS(UI_DEMO_IDLE_POLL_MS));
            }
            prev_sid = sid;
        }
    }
#else

    /* 初始化 ADC 波形接收队列（须在第一次 FetchFrame 之前完成） */
    UI_Waveform_QueueInit();

    /* 有效点数初始值 = ADC 帧大小（600 点），队列空时保持此值不变 */
    s_max_x = (ADC_FRAME_SIZE < (uint32_t)PLOT_W) ? (int)ADC_FRAME_SIZE : PLOT_W;

    MainTask_Scope_RestoreLayout();

    if (!DMA2D_PlotBuf_Init()) {
#if !UI_LIGHT_PATH_RELAX
        Error_Handler(__FILE__, __LINE__);
#else
        /* UI_LIGHT_PATH_RELAX=1：允许回退 GUI 绘制路径 */
#endif
    }

#if MAINTASK_PROFILE_EMWIN_SEGMENTS
    UI_MAIN_PRINTF("\r\n[MTP] MainTask profile ON, print every %u frames, see [6a]/[6b]\r\n",
           (unsigned)MAINTASK_PROFILE_ACCUM_FRAMES);
#endif

    {
        TickType_t frame_wake = xTaskGetTickCount();

        while (1) {
            if (!UI_Waveform_IsRunning()) {
                vTaskDelay(pdMS_TO_TICKS(50));
                frame_wake = xTaskGetTickCount();
            }
            MainTask_Scope_RenderFrame();
            if (UI_Waveform_IsRunning()) {
                vTaskDelayUntil(&frame_wake, pdMS_TO_TICKS(MAINTASK_FRAME_PERIOD_MS));
            }
        }
    }
#endif /* !UI_APP_DEMO_CYCLE_SCREENS */
}
