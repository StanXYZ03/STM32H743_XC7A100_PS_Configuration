#include "UI_WaveformCtrl.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>

/* ────────────────────────────────────────────────────────────────
 * 波形显示控制状态
 * ──────────────────────────────────────────────────────────────── */
static bool s_bIsRunning    = true;
static int  s_ch1_offset_px = 0;
static int  s_ch2_offset_px = 0;
static UI_Waveform_DisplayMode_t s_display_mode = UI_WAVEFORM_MODE_ROLL;
static UI_Waveform_Solo_t        s_solo          = UI_WAVEFORM_SOLO_BOTH;

/* 垂直偏移限幅（像素），与 MainTask 波形区高度同量级 */
#ifndef UI_WAVEFORM_CH_OFFSET_MAX
#define UI_WAVEFORM_CH_OFFSET_MAX  220
#endif

static void clamp_ch_offset(int *p)
{
    if (*p > UI_WAVEFORM_CH_OFFSET_MAX) {
        *p = UI_WAVEFORM_CH_OFFSET_MAX;
    }
    if (*p < -UI_WAVEFORM_CH_OFFSET_MAX) {
        *p = -UI_WAVEFORM_CH_OFFSET_MAX;
    }
}
static float s_volt_per_div = 1.0f;   /* V/div，默认 1V */
static float s_ms_per_div   = 1.0f;   /* ms/div，默认 1ms */

/* Roll 模式：环形缓冲，新数据从右侧追加，显示最后 N 个采样 */
#define ROLL_BUF_SIZE  (2u * ADC_FRAME_SIZE)
static int16_t s_roll_ch1[ROLL_BUF_SIZE];
static int16_t s_roll_ch2[ROLL_BUF_SIZE];
static uint32_t s_roll_cnt = 0u;   /* 当前有效采样数 */
static uint32_t s_roll_wr  = 0u;   /* 写入位置（环形） */

/* ────────────────────────────────────────────────────────────────
 * ADC 帧队列
 *   深度 = 1，配合 xQueueOverwrite 实现"最新帧覆盖"语义：
 *   生产者以任意速率发送，UI 以显示速率消费，
 *   始终只保留最新的一帧，不会积压延迟。
 * ──────────────────────────────────────────────────────────────── */
static QueueHandle_t s_qCH1 = NULL;
static QueueHandle_t s_qCH2 = NULL;

static volatile int s_stream_active = 1;

void UI_Waveform_SetStreamActive(int active)
{
    s_stream_active = active ? 1 : 0;
}

int UI_Waveform_IsStreamActive(void)
{
    return s_stream_active;
}

/* ── 初始化 ──────────────────────────────────────────────────── */

void UI_Waveform_QueueInit(void)
{
    if (s_qCH1 == NULL) {
        s_qCH1 = xQueueCreate(1u, sizeof(ADC_WaveFrame_t));
        configASSERT(s_qCH1 != NULL);
    }
    if (s_qCH2 == NULL) {
        s_qCH2 = xQueueCreate(1u, sizeof(ADC_WaveFrame_t));
        configASSERT(s_qCH2 != NULL);
    }
}

/* ── 内部辅助：填充帧结构体 ─────────────────────────────────── */

static void fill_frame(ADC_WaveFrame_t *pFrame,
                        const uint16_t  *adc_raw,
                        uint16_t         count)
{
    uint16_t n = (count > ADC_FRAME_SIZE) ? (uint16_t)ADC_FRAME_SIZE : count;
    memcpy(pFrame->data, adc_raw, (size_t)n * sizeof(uint16_t));
    pFrame->count = n;
}

/* ── 生产者：任务上下文 ──────────────────────────────────────── */

void UI_Waveform_SendCH1(const uint16_t *adc_raw, uint16_t count)
{
    ADC_WaveFrame_t frame;
    if (s_stream_active == 0) {
        return;
    }
    if (s_qCH1 == NULL) return;
    fill_frame(&frame, adc_raw, count);
    xQueueOverwrite(s_qCH1, &frame);
}

void UI_Waveform_SendCH2(const uint16_t *adc_raw, uint16_t count)
{
    ADC_WaveFrame_t frame;
    if (s_stream_active == 0) {
        return;
    }
    if (s_qCH2 == NULL) return;
    fill_frame(&frame, adc_raw, count);
    xQueueOverwrite(s_qCH2, &frame);
}

/* ── 生产者：ISR 上下文（DMA 传输完成回调等）────────────────── */

void UI_Waveform_SendCH1FromISR(const uint16_t *adc_raw,
                                 uint16_t        count,
                                 BaseType_t     *pHigherPriorityTaskWoken)
{
    ADC_WaveFrame_t frame;
    BaseType_t      dummy = pdFALSE;
    if (s_stream_active == 0) {
        return;
    }
    if (s_qCH1 == NULL) return;
    fill_frame(&frame, adc_raw, count);
    xQueueOverwriteFromISR(s_qCH1, &frame,
                           pHigherPriorityTaskWoken ? pHigherPriorityTaskWoken
                                                    : &dummy);
}

void UI_Waveform_SendCH2FromISR(const uint16_t *adc_raw,
                                 uint16_t        count,
                                 BaseType_t     *pHigherPriorityTaskWoken)
{
    ADC_WaveFrame_t frame;
    BaseType_t      dummy = pdFALSE;
    if (s_stream_active == 0) {
        return;
    }
    if (s_qCH2 == NULL) return;
    fill_frame(&frame, adc_raw, count);
    xQueueOverwriteFromISR(s_qCH2, &frame,
                           pHigherPriorityTaskWoken ? pHigherPriorityTaskWoken
                                                    : &dummy);
}

/* ── 消费者：MainTask 每帧调用 ───────────────────────────────── */

/*
 * ADC 原始值 → 显示单元
 *
 *   display_val = raw - 2048
 *
 * MainTask SampleToY：按 V/div 将 display_val 换为伏特再映射到像素（见 MainTask.c）。
 */
static inline int16_t adc_to_display(uint16_t raw)
{
    return (int16_t)((int32_t)raw - 2048);
}

/* 将新帧（CH1+CH2 成对）追加到 Roll 环形缓冲 */
static void roll_append(const ADC_WaveFrame_t *pCh1, const ADC_WaveFrame_t *pCh2)
{
    uint32_t n1 = (pCh1->count > ROLL_BUF_SIZE) ? ROLL_BUF_SIZE : pCh1->count;
    uint32_t n2 = (pCh2->count > ROLL_BUF_SIZE) ? ROLL_BUF_SIZE : pCh2->count;
    uint32_t n  = (n1 <= n2) ? n1 : n2;
    uint32_t i;
    for (i = 0; i < n; i++) {
        s_roll_ch1[s_roll_wr] = adc_to_display(pCh1->data[i]);
        s_roll_ch2[s_roll_wr] = adc_to_display(pCh2->data[i]);
        s_roll_wr = (s_roll_wr + 1u) % ROLL_BUF_SIZE;
    }
    s_roll_cnt += n;
    if (s_roll_cnt > ROLL_BUF_SIZE) s_roll_cnt = ROLL_BUF_SIZE;
}

/* 从 Roll 缓冲提取最后 buf_size 个采样到输出 */
static int roll_extract(int16_t *ch1_buf, int16_t *ch2_buf, int buf_size)
{
    uint32_t cnt = (s_roll_cnt > (uint32_t)buf_size) ? (uint32_t)buf_size : s_roll_cnt;
    if (cnt == 0u) return 0;

    /* 最后 cnt 个采样：起始位置 (s_roll_wr - cnt + ROLL_BUF_SIZE) % ROLL_BUF_SIZE */
    uint32_t start = (s_roll_wr + ROLL_BUF_SIZE - cnt) % ROLL_BUF_SIZE;
    uint32_t i;

    if (start + cnt <= ROLL_BUF_SIZE) {
        memcpy(ch1_buf, &s_roll_ch1[start], cnt * sizeof(int16_t));
        memcpy(ch2_buf, &s_roll_ch2[start], cnt * sizeof(int16_t));
    } else {
        uint32_t part1 = ROLL_BUF_SIZE - start;
        memcpy(ch1_buf, &s_roll_ch1[start], part1 * sizeof(int16_t));
        memcpy(ch1_buf + part1, s_roll_ch1, (cnt - part1) * sizeof(int16_t));
        memcpy(ch2_buf, &s_roll_ch2[start], part1 * sizeof(int16_t));
        memcpy(ch2_buf + part1, s_roll_ch2, (cnt - part1) * sizeof(int16_t));
    }
    return (int)cnt;
}

int UI_Waveform_FetchFrame(int16_t *ch1_buf, int16_t *ch2_buf, int buf_size)
{
    static ADC_WaveFrame_t s_fetch_roll_f1;
    static ADC_WaveFrame_t s_fetch_roll_f2;
    static ADC_WaveFrame_t s_fetch_yt;
    int got_ch1 = 0;
    int got_ch2 = 0;
    int i, n;

    /* STOP 状态：不消费队列，保持当前显示内容冻结 */
    if (!s_bIsRunning) {
        if (s_display_mode == UI_WAVEFORM_MODE_ROLL && s_roll_cnt > 0u)
            return roll_extract(ch1_buf, ch2_buf, buf_size);
        return 0;
    }

    if (s_qCH1 == NULL || s_qCH2 == NULL) return 0;

    if (s_display_mode == UI_WAVEFORM_MODE_ROLL) {
        /* Roll：勿在栈上同时放两个大帧（约 3.2KB+），易撑爆 emWin 任务栈 */
        int has_ch1 = (xQueueReceive(s_qCH1, &s_fetch_roll_f1, 0) == pdTRUE);
        int has_ch2 = (xQueueReceive(s_qCH2, &s_fetch_roll_f2, 0) == pdTRUE);
        if (has_ch1 && has_ch2)
            roll_append(&s_fetch_roll_f1, &s_fetch_roll_f2);
        return roll_extract(ch1_buf, ch2_buf, buf_size);
    }

    /* Y-T 模式：原逻辑，每帧完整覆盖 */
    if (xQueueReceive(s_qCH1, &s_fetch_yt, 0) == pdTRUE) {
        n = (int)s_fetch_yt.count;
        if (n > buf_size) n = buf_size;
        for (i = 0; i < n; i++) {
            ch1_buf[i] = adc_to_display(s_fetch_yt.data[i]);
        }
        got_ch1 = n;
    }
    if (xQueueReceive(s_qCH2, &s_fetch_yt, 0) == pdTRUE) {
        n = (int)s_fetch_yt.count;
        if (n > buf_size) n = buf_size;
        for (i = 0; i < n; i++) {
            ch2_buf[i] = adc_to_display(s_fetch_yt.data[i]);
        }
        got_ch2 = n;
    }
    return (got_ch1 >= got_ch2) ? got_ch1 : got_ch2;
}

/* ── 显示模式切换 ─────────────────────────────────────────────── */

void UI_Waveform_SetDisplayMode(UI_Waveform_DisplayMode_t mode)
{
    s_display_mode = mode;
    if (mode == UI_WAVEFORM_MODE_ROLL) {
        s_roll_cnt = 0u;
        s_roll_wr  = 0u;
    }
}

UI_Waveform_DisplayMode_t UI_Waveform_GetDisplayMode(void)
{
    return s_display_mode;
}

void UI_Waveform_ToggleDisplayMode(void)
{
    s_display_mode = (s_display_mode == UI_WAVEFORM_MODE_YT)
                     ? UI_WAVEFORM_MODE_ROLL : UI_WAVEFORM_MODE_YT;
    if (s_display_mode == UI_WAVEFORM_MODE_ROLL) {
        s_roll_cnt = 0u;
        s_roll_wr  = 0u;
    }
}

UI_Waveform_Solo_t UI_Waveform_GetSolo(void)
{
    return s_solo;
}

void UI_Waveform_SetSolo(UI_Waveform_Solo_t solo)
{
    if (solo <= UI_WAVEFORM_SOLO_CH2) {
        s_solo = solo;
    }
}

void UI_Waveform_CycleSolo(void)
{
    s_solo = (UI_Waveform_Solo_t)(((int)s_solo + 1) % 3);
}

const char *UI_Waveform_GetSoloLabel(void)
{
    switch (s_solo) {
    case UI_WAVEFORM_SOLO_CH1:
        return "CH1";
    case UI_WAVEFORM_SOLO_CH2:
        return "CH2";
    default:
        return "DUAL";
    }
}

/* ── 波形显示控制（原有逻辑保持不变）──────────────────────────── */

void UI_Waveform_ToggleRunStop(void)   { s_bIsRunning = !s_bIsRunning; }
bool UI_Waveform_IsRunning(void)       { return s_bIsRunning; }

void UI_Waveform_CH1_MoveUp(void)
{
    s_ch1_offset_px -= 10;
    clamp_ch_offset(&s_ch1_offset_px);
}

void UI_Waveform_CH1_MoveDown(void)
{
    s_ch1_offset_px += 10;
    clamp_ch_offset(&s_ch1_offset_px);
}

void UI_Waveform_CH2_MoveUp(void)
{
    s_ch2_offset_px -= 10;
    clamp_ch_offset(&s_ch2_offset_px);
}

void UI_Waveform_CH2_MoveDown(void)
{
    s_ch2_offset_px += 10;
    clamp_ch_offset(&s_ch2_offset_px);
}

int  UI_Waveform_GetCH1Offset(void)    { return s_ch1_offset_px; }
int  UI_Waveform_GetCH2Offset(void)    { return s_ch2_offset_px; }

/* ── 档位显示 ─────────────────────────────────────────────────── */

void UI_Waveform_SetVoltPerDiv(float v_per_div)
{
    if (v_per_div > 0.0f) s_volt_per_div = v_per_div;
}

void UI_Waveform_SetTimePerDiv(float ms_per_div)
{
    if (ms_per_div > 0.0f) s_ms_per_div = ms_per_div;
}

float UI_Waveform_GetVoltPerDiv(void) { return s_volt_per_div; }
float UI_Waveform_GetTimePerDiv(void) { return s_ms_per_div; }

void UI_Waveform_CycleVoltPerDiv(void)
{
    static const float tab[] = { 0.1f, 0.2f, 0.5f, 1.0f, 2.0f, 5.0f };
    const size_t       n     = sizeof(tab) / sizeof(tab[0]);
    size_t             i;
    size_t             best  = 0;
    float              best_d = 1e30f;

    for (i = 0; i < n; i++) {
        float d = s_volt_per_div - tab[i];
        if (d < 0.0f) {
            d = -d;
        }
        if (d < best_d) {
            best_d = d;
            best   = i;
        }
    }
    best = (best + 1u) % n;
    s_volt_per_div = tab[best];
}

void UI_Waveform_CycleTimePerDiv(void)
{
    static const float tab[] = {
        0.05f, 0.1f, 0.2f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f
    };
    const size_t n    = sizeof(tab) / sizeof(tab[0]);
    size_t       i;
    size_t       best  = 0;
    float        best_d = 1e30f;

    for (i = 0; i < n; i++) {
        float d = s_ms_per_div - tab[i];
        if (d < 0.0f) {
            d = -d;
        }
        if (d < best_d) {
            best_d = d;
            best   = i;
        }
    }
    best = (best + 1u) % n;
    s_ms_per_div = tab[best];
}

void UI_Waveform_FormatScaleStr(char *buf, uint32_t buf_size)
{
    uint32_t len;
    if (!buf || buf_size < 20u) return;
    if (s_volt_per_div >= 1.0f)
        len = (uint32_t)sprintf(buf, "%.0fV/div  ", (double)s_volt_per_div);
    else
        len = (uint32_t)sprintf(buf, "%.0fmV/div ", (double)(s_volt_per_div * 1000.0f));
    if (len >= buf_size) return;
    if (s_ms_per_div >= 1.0f)
        (void)sprintf(buf + len, "%.0fms/div", (double)s_ms_per_div);
    else if (s_ms_per_div >= 0.001f)
        (void)sprintf(buf + len, "%.0fus/div", (double)(s_ms_per_div * 1000.0f));
    else
        (void)sprintf(buf + len, "%.0fns/div", (double)(s_ms_per_div * 1000000.0f));
}
