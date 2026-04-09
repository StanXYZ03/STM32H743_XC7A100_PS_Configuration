#ifndef UI_WAVEFORM_CTRL_H
#define UI_WAVEFORM_CTRL_H

#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "queue.h"

/* ── 显示模式：Y-T / Roll（默认 Roll，上电即滚动；仍可调 Set/Toggle） ── */

typedef enum {
    UI_WAVEFORM_MODE_YT,    /* Y-T 模式：每帧完整刷新，传统示波器 */
    UI_WAVEFORM_MODE_ROLL   /* Roll 模式：新数据从右侧进入，向左滚动（纸带式） */
} UI_Waveform_DisplayMode_t;

/** 设置显示模式（Y-T / Roll），可从外部任务或按键回调调用 */
void UI_Waveform_SetDisplayMode(UI_Waveform_DisplayMode_t mode);

/** 获取当前显示模式 */
UI_Waveform_DisplayMode_t UI_Waveform_GetDisplayMode(void);

/** 在 Y-T 与 Roll 之间切换 */
void UI_Waveform_ToggleDisplayMode(void);

/** 波形区单独显示：双通道 / 仅 CH1 / 仅 CH2 */
typedef enum {
    UI_WAVEFORM_SOLO_BOTH = 0,
    UI_WAVEFORM_SOLO_CH1,
    UI_WAVEFORM_SOLO_CH2
} UI_Waveform_Solo_t;

UI_Waveform_Solo_t UI_Waveform_GetSolo(void);
void UI_Waveform_SetSolo(UI_Waveform_Solo_t solo);
/** 顺序：BOTH → CH1 → CH2 → BOTH */
void UI_Waveform_CycleSolo(void);
/** 简短标签："DUAL" / "CH1" / "CH2" */
const char *UI_Waveform_GetSoloLabel(void);

/* ── 波形显示控制 ─────────────────────────────────────────────── */

bool UI_Waveform_IsRunning(void);
void UI_Waveform_ToggleRunStop(void);

void UI_Waveform_CH1_MoveUp(void);
void UI_Waveform_CH1_MoveDown(void);
void UI_Waveform_CH2_MoveUp(void);
void UI_Waveform_CH2_MoveDown(void);

int  UI_Waveform_GetCH1Offset(void);
int  UI_Waveform_GetCH2Offset(void);

/* ── 档位显示（V/div、ms/div）──────────────────────────────────────
 * 用于 Header 显示，可随实际档位变化（当前为可配置常量）            */

/** 设置电压档位（V/div），如 1.0f = "1V/div", 0.1f = "100mV/div" */
void UI_Waveform_SetVoltPerDiv(float v_per_div);

/** 设置时间档位（ms/div），如 1.0f = "1ms/div", 0.1f = "100us/div" */
void UI_Waveform_SetTimePerDiv(float ms_per_div);

/** 获取电压档位 */
float UI_Waveform_GetVoltPerDiv(void);

/** 获取时间档位 */
float UI_Waveform_GetTimePerDiv(void);

/** 格式化档位字符串到 buf，如 "1V/div  1ms/div"，buf 至少 24 字节 */
void UI_Waveform_FormatScaleStr(char *buf, uint32_t buf_size);

/** 按预设档位循环 V/div（波形页长按等） */
void UI_Waveform_CycleVoltPerDiv(void);

/** 按预设档位循环 ms/div（波形页长按等） */
void UI_Waveform_CycleTimePerDiv(void);

/* ── ADC 波形数据队列接口 ─────────────────────────────────────── */

/** ADC 位宽（8 / 10 / 12 / 14 / 16） */
#define ADC_BITS        8u
#define ADC_MID         (1u << (ADC_BITS - 1u))
#define ADC_MAX         ((1u << ADC_BITS) - 1u)

/** 每帧固定采样点数 */
#define ADC_FRAME_SIZE  800u

/**
 * ADC 帧数据结构体。
 * count：本次实际有效点数（≤ ADC_FRAME_SIZE），data[count..] 无效。
 * 生产者填充此结构体后调用 UI_Waveform_SendCHx 发送。
 */
typedef struct {
    uint16_t data[ADC_FRAME_SIZE]; /* ADC 原始值 (0 ~ ADC_MAX) */
    uint16_t count;                /* 实际有效点数，通常 = ADC_FRAME_SIZE */
} ADC_WaveFrame_t;

/**
 * 初始化 CH1 / CH2 接收队列。
 * 必须在 vTaskStartScheduler() 之前或任务初始化阶段调用一次。
 */
void UI_Waveform_QueueInit(void);

/* ── 生产者接口（ADC 任务 / DMA 完成回调）─────────────────────── */

/**
 * 从任务上下文发送一帧原始 ADC 数据。
 * 若 UI 尚未消费上一帧，则覆盖（永不阻塞）。
 * @param adc_raw  指向 ADC 原始值数组（ADC_BITS 位, 0~ADC_MAX）
 * @param count    有效点数（> ADC_FRAME_SIZE 时自动截断）
 */
void UI_Waveform_SendCH1(const uint16_t *adc_raw, uint16_t count);
void UI_Waveform_SendCH2(const uint16_t *adc_raw, uint16_t count);

/**
 * 从 ISR 上下文发送一帧（DMA 传输完成中断等）。
 * @param pHigherPriorityTaskWoken  FreeRTOS 标准唤醒标志，传入 NULL 亦可
 */
void UI_Waveform_SendCH1FromISR(const uint16_t *adc_raw, uint16_t count,
                                 BaseType_t *pHigherPriorityTaskWoken);
void UI_Waveform_SendCH2FromISR(const uint16_t *adc_raw, uint16_t count,
                                 BaseType_t *pHigherPriorityTaskWoken);

/* ── 消费者接口（MainTask 每帧调用）──────────────────────────── */

/**
 * 非阻塞地从队列取出最新一帧，转换为 emWin 显示单元后写入外部缓冲。
 *
 * 转换公式：display_val = (int16_t)(raw - ADC_MID)
 *   配合 SampleToY 的缩放，ADC 满量程对应屏幕上下约 ±64 px 偏移（8 位时）。
 *
 * 若波形处于 STOP 状态（IsRunning==false），函数直接返回 0，
 * 保持外部缓冲不变（冻结显示）。
 *
 * @param ch1_buf   CH1 显示缓冲，容量须 ≥ buf_size（int16_t 数组）
 * @param ch2_buf   CH2 显示缓冲，容量须 ≥ buf_size
 * @param buf_size  缓冲容量（点数），建议传 WAVEFORM_BUFFER_SIZE
 * @return          本次写入的有效点数；0 表示队列空或处于 STOP 状态
 */
int UI_Waveform_FetchFrame(int16_t *ch1_buf, int16_t *ch2_buf, int buf_size);

/**
 * 波形数据流开关：0 时 SendCHx / SendCHxFromISR 不写队列（非示波器页静默）。
 * 默认 1。多页 UI 在离开示波器时置 0，假 ADC 任务可配合休眠。
 */
void UI_Waveform_SetStreamActive(int active);
int  UI_Waveform_IsStreamActive(void);

#endif /* UI_WAVEFORM_CTRL_H */
