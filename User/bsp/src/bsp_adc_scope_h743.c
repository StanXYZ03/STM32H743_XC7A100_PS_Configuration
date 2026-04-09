#include "bsp.h"
#include "bsp_adc_scope_h743.h"

#if (SCOPE_SEND_TO_UI_WAVEFORM == 1)
#include "UI_WaveformCtrl.h"
#endif

#ifndef SCOPE_RATECAPS_TYPEDEF_DEFINED
typedef struct
{
    uint32_t max_trigger_hz;
    uint32_t max_ch1_hz;
    uint32_t max_ch2_hz;
} SCOPE_RateCapsTypeDef;
#endif

#define SCOPE_USE_DMA_IRQ   1

#if defined(__CC_ARM)
    #define SCOPE_ALIGN32      __align(32)
    #define SCOPE_DMA_RAM      __attribute__((section(".RAM_D1")))
#else
    #define SCOPE_ALIGN32      __attribute__((aligned(32)))
    #define SCOPE_DMA_RAM      __attribute__((section(".RAM_D1")))
#endif

static ADC_HandleTypeDef hscope_adc1 = {0};
static ADC_HandleTypeDef hscope_adc2 = {0};
static ADC_HandleTypeDef hscope_adc3 = {0};

static DMA_HandleTypeDef hscope_dma_adc12 = {0};
static DMA_HandleTypeDef hscope_dma_adc3 = {0};

static TIM_HandleTypeDef hscope_tim8 = {0};

static volatile uint8_t g_scope_running = 0U;
static SCOPE_InitTypeDef g_scope_cfg = {0};
static SCOPE_DmaStatsTypeDef g_scope_dma_stats = {0};
#if !SCOPE_USE_DMA_IRQ
static uint32_t g_scope_last_ndtr_adc12 = 0xFFFFFFFFU;
static uint32_t g_scope_last_ndtr_adc3 = 0xFFFFFFFFU;
#endif
static uint32_t g_scope_last_dma_stats_sum = 0xFFFFFFFFU;

SCOPE_ALIGN32 static uint32_t g_scope_ch1_packed[SCOPE_CH1_PACKED_LEN] SCOPE_DMA_RAM;
SCOPE_ALIGN32 static uint16_t g_scope_ch2_samples[SCOPE_CH2_SAMPLES_LEN] SCOPE_DMA_RAM;

#if (SCOPE_SEND_TO_UI_WAVEFORM == 1)
/* 半区/满区各一套缓冲，保证同一触发、同一缓冲中 CH1/CH2 成对发送 */
static uint16_t s_ui_ch1_half[ADC_FRAME_SIZE];
static uint16_t s_ui_ch2_half[ADC_FRAME_SIZE];
static uint16_t s_ui_ch1_full[ADC_FRAME_SIZE];
static uint16_t s_ui_ch2_full[ADC_FRAME_SIZE];
static volatile uint8_t s_ch1_half_ready = 0U;
static volatile uint8_t s_ch2_half_ready = 0U;
static volatile uint8_t s_ch1_full_ready = 0U;
static volatile uint8_t s_ch2_full_ready = 0U;

/* 从 CH1 打包区抽取到 out，packed_len 为该半区 uint32_t 数量 */
static void scope_decimate_ch1(const uint32_t *packed, uint32_t packed_len, uint16_t *out)
{
    uint32_t total_samples = packed_len * 2U;
    uint32_t i;
    for (i = 0U; i < ADC_FRAME_SIZE; i++) {
        uint32_t src_idx = (i * total_samples) / ADC_FRAME_SIZE;
        uint32_t pidx = src_idx / 2U;
        uint32_t sub = src_idx & 1U;
        out[i] = (uint16_t)(sub ? ((packed[pidx] >> 16U) & 0xFFFFU) : (packed[pidx] & 0xFFFFU));
    }
}

/* 从 CH2 半区抽取到 out */
static void scope_decimate_ch2(const uint16_t *samples, uint32_t sample_len, uint16_t *out)
{
    uint32_t i;
    for (i = 0U; i < ADC_FRAME_SIZE; i++) {
        uint32_t src_idx = (i * sample_len) / ADC_FRAME_SIZE;
        out[i] = samples[src_idx];
    }
}

/* 成对发送：仅当 CH1、CH2 均就绪时发送，满足 Roll 模式 has_ch1 && has_ch2 */
static void scope_send_pair_if_ready(uint16_t *ch1_buf, uint16_t *ch2_buf,
                                    volatile uint8_t *ch1_ready, volatile uint8_t *ch2_ready)
{
    if (*ch1_ready && *ch2_ready) {
        UI_Waveform_SendCH1FromISR(ch1_buf, ADC_FRAME_SIZE, NULL);
        UI_Waveform_SendCH2FromISR(ch2_buf, ADC_FRAME_SIZE, NULL);
        *ch1_ready = 0U;
        *ch2_ready = 0U;
    }
}
#endif

static HAL_StatusTypeDef SCOPE_ConfigAdcClock(uint32_t adc_clock_hz);
static HAL_StatusTypeDef SCOPE_ConfigTriggerTimer(uint32_t trigger_hz);
static HAL_StatusTypeDef SCOPE_ConfigAdcChannels(const SCOPE_InitTypeDef *cfg);
static HAL_StatusTypeDef SCOPE_ConfigMultiMode(uint32_t resolution, uint32_t two_sampling_delay);
static uint32_t SCOPE_GetTim8Clock(void);
static HAL_StatusTypeDef SCOPE_ValidateConfig(const SCOPE_InitTypeDef *cfg);
static uint32_t SCOPE_GetResolutionConvCycles(uint32_t resolution);
static uint32_t SCOPE_GetSampleCyclesX2(uint32_t sampling_time);
static uint32_t SCOPE_GetTwoSamplingDelayCycles(uint32_t two_sampling_delay);
static uint32_t SCOPE_GetMaxTriggerHzByConfig(const SCOPE_InitTypeDef *cfg);
static uint32_t SCOPE_GetMaxAdcClockByResolution(uint32_t resolution);

void SCOPE_InitDefaultConfig(SCOPE_InitTypeDef *cfg)
{
    if (cfg == NULL)
    {
        return;
    }

    cfg->trigger_hz = 1000000U;
    cfg->adc_clock_hz = 50000000U;
    cfg->resolution = ADC_RESOLUTION_12B;
    cfg->pa6_sampling_time = ADC_SAMPLETIME_1CYCLE_5;
    cfg->pc3_sampling_time = ADC_SAMPLETIME_8CYCLES_5;
    cfg->two_sampling_delay = ADC_TWOSAMPLINGDELAY_5CYCLES;
    cfg->ch2_input = SCOPE_CH2_INPUT_PC3;
    cfg->ch2_trigger_mode = SCOPE_CH2_TRIG_MIDPOINT_TRGO2;
}

void SCOPE_ApplyResolutionPreset(SCOPE_InitTypeDef *cfg, uint32_t resolution)
{
    if (cfg == NULL)
    {
        return;
    }

    cfg->resolution = resolution;

    if (resolution == ADC_RESOLUTION_16B)
    {
        cfg->adc_clock_hz = 36000000U;
        cfg->pa6_sampling_time = ADC_SAMPLETIME_1CYCLE_5;
        cfg->pc3_sampling_time = ADC_SAMPLETIME_8CYCLES_5;
        cfg->two_sampling_delay = ADC_TWOSAMPLINGDELAY_5CYCLES;
    }
    else if (resolution == ADC_RESOLUTION_14B)
    {
        cfg->adc_clock_hz = 50000000U;
        cfg->pa6_sampling_time = ADC_SAMPLETIME_2CYCLES_5;
        cfg->pc3_sampling_time = ADC_SAMPLETIME_8CYCLES_5;
        cfg->two_sampling_delay = ADC_TWOSAMPLINGDELAY_5CYCLES;
    }
    else if (resolution == ADC_RESOLUTION_12B)
    {
        cfg->adc_clock_hz = 50000000U;
        cfg->pa6_sampling_time = ADC_SAMPLETIME_2CYCLES_5;
        cfg->pc3_sampling_time = ADC_SAMPLETIME_8CYCLES_5;
        cfg->two_sampling_delay = ADC_TWOSAMPLINGDELAY_5CYCLES;
    }
    else if (resolution == ADC_RESOLUTION_10B)
    {
        cfg->adc_clock_hz = 50000000U;
        cfg->pa6_sampling_time = ADC_SAMPLETIME_1CYCLE_5;
        cfg->pc3_sampling_time = ADC_SAMPLETIME_8CYCLES_5;
        cfg->two_sampling_delay = ADC_TWOSAMPLINGDELAY_4CYCLES;
    }
    else if (resolution == ADC_RESOLUTION_8B)
    {
        cfg->adc_clock_hz = 50000000U;
        cfg->pa6_sampling_time = ADC_SAMPLETIME_1CYCLE_5;
        cfg->pc3_sampling_time = ADC_SAMPLETIME_8CYCLES_5;
        cfg->two_sampling_delay = ADC_TWOSAMPLINGDELAY_3CYCLES;
    }
    else
    {
        cfg->resolution = ADC_RESOLUTION_12B;
        cfg->adc_clock_hz = 50000000U;
        cfg->pa6_sampling_time = ADC_SAMPLETIME_2CYCLES_5;
        cfg->pc3_sampling_time = ADC_SAMPLETIME_8CYCLES_5;
        cfg->two_sampling_delay = ADC_TWOSAMPLINGDELAY_5CYCLES;
    }
}

HAL_StatusTypeDef SCOPE_GetRateCaps(const SCOPE_InitTypeDef *cfg, SCOPE_RateCapsTypeDef *caps)
{
    uint32_t conv_cycles;
    uint32_t pa6_cycles_x2;
    uint32_t ch2_cycles_x2;
    uint32_t tconv_ch2_x2;

    if ((cfg == NULL) || (caps == NULL))
    {
        return HAL_ERROR;
    }

    conv_cycles = SCOPE_GetResolutionConvCycles(cfg->resolution);
    pa6_cycles_x2 = SCOPE_GetSampleCyclesX2(cfg->pa6_sampling_time);
    ch2_cycles_x2 = SCOPE_GetSampleCyclesX2(cfg->pc3_sampling_time);
    if ((conv_cycles == 0U) || (pa6_cycles_x2 == 0U) || (ch2_cycles_x2 == 0U))
    {
        return HAL_ERROR;
    }

    tconv_ch2_x2 = ch2_cycles_x2 + 1U + (2U * conv_cycles);

    caps->max_trigger_hz = SCOPE_GetMaxTriggerHzByConfig(cfg);
    caps->max_ch1_hz = caps->max_trigger_hz * 2U;
    caps->max_ch2_hz = (uint32_t)(((uint64_t)cfg->adc_clock_hz * 2ULL) / (uint64_t)tconv_ch2_x2);

    return HAL_OK;
}

HAL_StatusTypeDef SCOPE_Start(const SCOPE_InitTypeDef *cfg)
{
    HAL_StatusTypeDef ret;
    FunctionalState boost_enable;

    if (cfg == NULL)
    {
        return HAL_ERROR;
    }

    if ((cfg->trigger_hz == 0U) || (cfg->adc_clock_hz == 0U))
    {
        return HAL_ERROR;
    }

    if (SCOPE_ValidateConfig(cfg) != HAL_OK)
    {
        return HAL_ERROR;
    }

    if (g_scope_running != 0U)
    {
        SCOPE_Stop();
    }

    g_scope_cfg = *cfg;
    memset((void *)&g_scope_dma_stats, 0, sizeof(g_scope_dma_stats));
#if !SCOPE_USE_DMA_IRQ
    g_scope_last_ndtr_adc12 = 0xFFFFFFFFU;
    g_scope_last_ndtr_adc3 = 0xFFFFFFFFU;
#endif
    g_scope_last_dma_stats_sum = 0xFFFFFFFFU;

    ret = SCOPE_ConfigAdcClock(g_scope_cfg.adc_clock_hz);
    if (ret != HAL_OK)
    {
        return ret;
    }

    ret = SCOPE_ConfigTriggerTimer(g_scope_cfg.trigger_hz);
    if (ret != HAL_OK)
    {
        return ret;
    }

    boost_enable = (g_scope_cfg.adc_clock_hz > 20000000U) ? ENABLE : DISABLE;

    if (hscope_adc1.Instance == ADC1)
    {
        (void)HAL_ADC_DeInit(&hscope_adc1);
    }
    if (hscope_adc2.Instance == ADC2)
    {
        (void)HAL_ADC_DeInit(&hscope_adc2);
    }
    if (hscope_adc3.Instance == ADC3)
    {
        (void)HAL_ADC_DeInit(&hscope_adc3);
    }

    memset(&hscope_adc1, 0, sizeof(hscope_adc1));
    memset(&hscope_adc2, 0, sizeof(hscope_adc2));
    memset(&hscope_adc3, 0, sizeof(hscope_adc3));

    hscope_adc1.Instance = ADC1;
    hscope_adc2.Instance = ADC2;
    hscope_adc3.Instance = ADC3;

    hscope_adc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
    hscope_adc1.Init.Resolution = g_scope_cfg.resolution;
    hscope_adc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hscope_adc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    hscope_adc1.Init.LowPowerAutoWait = DISABLE;
    hscope_adc1.Init.ContinuousConvMode = DISABLE;
    hscope_adc1.Init.NbrOfConversion = 1U;
    hscope_adc1.Init.DiscontinuousConvMode = DISABLE;
    hscope_adc1.Init.NbrOfDiscConversion = 1U;
    hscope_adc1.Init.ExternalTrigConv = ADC_EXTERNALTRIG_T8_TRGO;
    hscope_adc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
    hscope_adc1.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DMA_CIRCULAR;
    hscope_adc1.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
    hscope_adc1.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
    hscope_adc1.Init.BoostMode = boost_enable;
    hscope_adc1.Init.OversamplingMode = DISABLE;
    if (HAL_ADC_Init(&hscope_adc1) != HAL_OK)
    {
        return HAL_ERROR;
    }

    hscope_adc2.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
    hscope_adc2.Init.Resolution = g_scope_cfg.resolution;
    hscope_adc2.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hscope_adc2.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    hscope_adc2.Init.LowPowerAutoWait = DISABLE;
    hscope_adc2.Init.ContinuousConvMode = DISABLE;
    hscope_adc2.Init.NbrOfConversion = 1U;
    hscope_adc2.Init.DiscontinuousConvMode = DISABLE;
    hscope_adc2.Init.NbrOfDiscConversion = 1U;
    hscope_adc2.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hscope_adc2.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hscope_adc2.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;
    hscope_adc2.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
    hscope_adc2.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
    hscope_adc2.Init.BoostMode = boost_enable;
    hscope_adc2.Init.OversamplingMode = DISABLE;
    if (HAL_ADC_Init(&hscope_adc2) != HAL_OK)
    {
        return HAL_ERROR;
    }

    hscope_adc3.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
    hscope_adc3.Init.Resolution = g_scope_cfg.resolution;
    hscope_adc3.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hscope_adc3.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    hscope_adc3.Init.LowPowerAutoWait = DISABLE;
    hscope_adc3.Init.ContinuousConvMode = DISABLE;
    hscope_adc3.Init.NbrOfConversion = 1U;
    hscope_adc3.Init.DiscontinuousConvMode = DISABLE;
    hscope_adc3.Init.NbrOfDiscConversion = 1U;
    if (g_scope_cfg.ch2_trigger_mode == SCOPE_CH2_TRIG_SYNC_TRGO)
    {
        hscope_adc3.Init.ExternalTrigConv = ADC_EXTERNALTRIG_T8_TRGO;
    }
    else
    {
        hscope_adc3.Init.ExternalTrigConv = ADC_EXTERNALTRIG_T8_TRGO2;
    }
    hscope_adc3.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
    hscope_adc3.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DMA_CIRCULAR;
    hscope_adc3.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
    hscope_adc3.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
    hscope_adc3.Init.BoostMode = boost_enable;
    hscope_adc3.Init.OversamplingMode = DISABLE;
    if (HAL_ADC_Init(&hscope_adc3) != HAL_OK)
    {
        return HAL_ERROR;
    }

    ret = SCOPE_ConfigAdcChannels(&g_scope_cfg);
    if (ret != HAL_OK)
    {
        return ret;
    }

    ret = SCOPE_ConfigMultiMode(g_scope_cfg.resolution, g_scope_cfg.two_sampling_delay);
    if (ret != HAL_OK)
    {
        return ret;
    }

    if (HAL_ADCEx_Calibration_Start(&hscope_adc1, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED) != HAL_OK)
    {
        return HAL_ERROR;
    }
    if (HAL_ADCEx_Calibration_Start(&hscope_adc2, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED) != HAL_OK)
    {
        return HAL_ERROR;
    }
    if (HAL_ADCEx_Calibration_Start(&hscope_adc3, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED) != HAL_OK)
    {
        return HAL_ERROR;
    }

    if (HAL_ADC_Start_DMA(&hscope_adc3, (uint32_t *)g_scope_ch2_samples, SCOPE_CH2_SAMPLES_LEN) != HAL_OK)
    {
        return HAL_ERROR;
    }

    if (HAL_ADC_Start(&hscope_adc2) != HAL_OK)
    {
        HAL_ADC_Stop_DMA(&hscope_adc3);
        return HAL_ERROR;
    }

    if (HAL_ADCEx_MultiModeStart_DMA(&hscope_adc1, g_scope_ch1_packed, SCOPE_CH1_PACKED_LEN) != HAL_OK)
    {
        HAL_ADC_Stop(&hscope_adc2);
        HAL_ADC_Stop_DMA(&hscope_adc3);
        return HAL_ERROR;
    }

    if (HAL_TIM_Base_Start(&hscope_tim8) != HAL_OK)
    {
        HAL_ADCEx_MultiModeStop_DMA(&hscope_adc1);
        HAL_ADC_Stop(&hscope_adc2);
        HAL_ADC_Stop_DMA(&hscope_adc3);
        return HAL_ERROR;
    }

    if ((g_scope_cfg.ch2_trigger_mode == SCOPE_CH2_TRIG_MIDPOINT_TRGO2) &&
        (HAL_TIM_OC_Start(&hscope_tim8, TIM_CHANNEL_1) != HAL_OK))
    {
        HAL_TIM_Base_Stop(&hscope_tim8);
        HAL_ADCEx_MultiModeStop_DMA(&hscope_adc1);
        HAL_ADC_Stop(&hscope_adc2);
        HAL_ADC_Stop_DMA(&hscope_adc3);
        return HAL_ERROR;
    }

    g_scope_running = 1U;
    return HAL_OK;
}

void SCOPE_Stop(void)
{
    if (hscope_tim8.Instance == TIM8)
    {
        if (g_scope_cfg.ch2_trigger_mode == SCOPE_CH2_TRIG_MIDPOINT_TRGO2)
        {
            HAL_TIM_OC_Stop(&hscope_tim8, TIM_CHANNEL_1);
        }
        HAL_TIM_Base_Stop(&hscope_tim8);
    }

    if (hscope_adc1.Instance == ADC1)
    {
        HAL_ADCEx_MultiModeStop_DMA(&hscope_adc1);
        HAL_ADC_Stop(&hscope_adc1);
    }

    if (hscope_adc2.Instance == ADC2)
    {
        HAL_ADC_Stop(&hscope_adc2);
    }

    if (hscope_adc3.Instance == ADC3)
    {
        HAL_ADC_Stop_DMA(&hscope_adc3);
        HAL_ADC_Stop(&hscope_adc3);
    }

    g_scope_running = 0U;
#if !SCOPE_USE_DMA_IRQ
    g_scope_last_ndtr_adc12 = 0xFFFFFFFFU;
    g_scope_last_ndtr_adc3 = 0xFFFFFFFFU;
#endif
    g_scope_last_dma_stats_sum = 0xFFFFFFFFU;
}

uint8_t SCOPE_IsRunning(void)
{
    return g_scope_running;
}

void SCOPE_GetBuffers(const uint32_t **ch1_packed,
                      uint32_t *ch1_len,
                      const uint16_t **ch2,
                      uint32_t *ch2_len)
{
    if (ch1_packed != NULL)
    {
        *ch1_packed = g_scope_ch1_packed;
    }
    if (ch1_len != NULL)
    {
        *ch1_len = SCOPE_CH1_PACKED_LEN;
    }
    if (ch2 != NULL)
    {
        *ch2 = g_scope_ch2_samples;
    }
    if (ch2_len != NULL)
    {
        *ch2_len = SCOPE_CH2_SAMPLES_LEN;
    }
}

void SCOPE_UnpackCh1(const uint32_t *packed,
                     uint32_t packed_len,
                     uint16_t *out_samples,
                     uint32_t out_len)
{
    uint32_t i;
    uint32_t need_len;

    if ((packed == NULL) || (out_samples == NULL))
    {
        return;
    }

    need_len = packed_len * 2U;
    if (out_len < need_len)
    {
        return;
    }

    for (i = 0U; i < packed_len; i++)
    {
        out_samples[2U * i] = (uint16_t)(packed[i] & 0xFFFFU);
        out_samples[2U * i + 1U] = (uint16_t)((packed[i] >> 16U) & 0xFFFFU);
    }
}

void SCOPE_GetDmaStats(SCOPE_DmaStatsTypeDef *stats)
{
    if (stats == NULL)
    {
        return;
    }

    *stats = g_scope_dma_stats;
}

void SCOPE_ClearDmaStats(void)
{
    memset((void *)&g_scope_dma_stats, 0, sizeof(g_scope_dma_stats));
    g_scope_last_dma_stats_sum = 0xFFFFFFFFU;
}

uint8_t SCOPE_IsDmaProgressing(void)
{
#if SCOPE_USE_DMA_IRQ
    uint32_t current_sum;

    if (g_scope_running == 0U)
    {
        return 0U;
    }

    current_sum = g_scope_dma_stats.adc12_half_count +
                  g_scope_dma_stats.adc12_full_count +
                  g_scope_dma_stats.adc3_half_count +
                  g_scope_dma_stats.adc3_full_count;

    if ((g_scope_last_dma_stats_sum == 0xFFFFFFFFU) || (current_sum != g_scope_last_dma_stats_sum))
    {
        g_scope_last_dma_stats_sum = current_sum;
        return 1U;
    }

    return 0U;
#else
    uint32_t ndtr12;
    uint32_t ndtr3;
    uint8_t moving;

    if (g_scope_running == 0U)
    {
        return 0U;
    }

    ndtr12 = __HAL_DMA_GET_COUNTER(&hscope_dma_adc12);
    ndtr3 = __HAL_DMA_GET_COUNTER(&hscope_dma_adc3);

    moving = 0U;
    if ((g_scope_last_ndtr_adc12 == 0xFFFFFFFFU) || (g_scope_last_ndtr_adc3 == 0xFFFFFFFFU))
    {
        moving = 1U;
    }
    else if ((ndtr12 != g_scope_last_ndtr_adc12) && (ndtr3 != g_scope_last_ndtr_adc3))
    {
        moving = 1U;
    }

    g_scope_last_ndtr_adc12 = ndtr12;
    g_scope_last_ndtr_adc3 = ndtr3;
    return moving;
#endif
}

static HAL_StatusTypeDef SCOPE_ValidateConfig(const SCOPE_InitTypeDef *cfg)
{
    uint32_t max_adc_clock_hz;
    uint32_t max_trigger_hz;

    if ((cfg->ch2_input != SCOPE_CH2_INPUT_PC2) && (cfg->ch2_input != SCOPE_CH2_INPUT_PC3))
    {
        return HAL_ERROR;
    }
    if ((cfg->ch2_trigger_mode != SCOPE_CH2_TRIG_SYNC_TRGO) &&
        (cfg->ch2_trigger_mode != SCOPE_CH2_TRIG_MIDPOINT_TRGO2))
    {
        return HAL_ERROR;
    }

    if (SCOPE_GetResolutionConvCycles(cfg->resolution) == 0U)
    {
        return HAL_ERROR;
    }

    if (SCOPE_GetSampleCyclesX2(cfg->pa6_sampling_time) == 0U)
    {
        return HAL_ERROR;
    }

    if (SCOPE_GetSampleCyclesX2(cfg->pc3_sampling_time) == 0U)
    {
        return HAL_ERROR;
    }

    if (SCOPE_GetTwoSamplingDelayCycles(cfg->two_sampling_delay) == 0U)
    {
        return HAL_ERROR;
    }

    max_adc_clock_hz = SCOPE_GetMaxAdcClockByResolution(cfg->resolution);
    if (max_adc_clock_hz == 0U)
    {
        return HAL_ERROR;
    }

    if (cfg->adc_clock_hz > max_adc_clock_hz)
    {
        return HAL_ERROR;
    }

    max_trigger_hz = SCOPE_GetMaxTriggerHzByConfig(cfg);
    if ((max_trigger_hz == 0U) || (cfg->trigger_hz > max_trigger_hz))
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

static uint32_t SCOPE_GetMaxAdcClockByResolution(uint32_t resolution)
{
    if (resolution == ADC_RESOLUTION_16B)
    {
        return 36000000U;
    }

    if ((resolution == ADC_RESOLUTION_14B) ||
        (resolution == ADC_RESOLUTION_12B) ||
        (resolution == ADC_RESOLUTION_10B) ||
        (resolution == ADC_RESOLUTION_8B))
    {
        return 50000000U;
    }

    return 0U;
}

static uint32_t SCOPE_GetResolutionConvCycles(uint32_t resolution)
{
    if (resolution == ADC_RESOLUTION_16B)
    {
        return 8U;
    }
    if (resolution == ADC_RESOLUTION_14B)
    {
        return 7U;
    }
    if (resolution == ADC_RESOLUTION_12B)
    {
        return 6U;
    }
    if (resolution == ADC_RESOLUTION_10B)
    {
        return 5U;
    }
    if (resolution == ADC_RESOLUTION_8B)
    {
        return 4U;
    }

    return 0U;
}

static uint32_t SCOPE_GetSampleCyclesX2(uint32_t sampling_time)
{
    if (sampling_time == ADC_SAMPLETIME_1CYCLE_5)
    {
        return 3U;
    }
    if (sampling_time == ADC_SAMPLETIME_2CYCLES_5)
    {
        return 5U;
    }
    if (sampling_time == ADC_SAMPLETIME_8CYCLES_5)
    {
        return 17U;
    }
    if (sampling_time == ADC_SAMPLETIME_16CYCLES_5)
    {
        return 33U;
    }
    if (sampling_time == ADC_SAMPLETIME_32CYCLES_5)
    {
        return 65U;
    }
    if (sampling_time == ADC_SAMPLETIME_64CYCLES_5)
    {
        return 129U;
    }
    if (sampling_time == ADC_SAMPLETIME_387CYCLES_5)
    {
        return 775U;
    }
    if (sampling_time == ADC_SAMPLETIME_810CYCLES_5)
    {
        return 1621U;
    }

    return 0U;
}

static uint32_t SCOPE_GetTwoSamplingDelayCycles(uint32_t two_sampling_delay)
{
    if (two_sampling_delay == ADC_TWOSAMPLINGDELAY_1CYCLE)
    {
        return 1U;
    }
    if (two_sampling_delay == ADC_TWOSAMPLINGDELAY_2CYCLES)
    {
        return 2U;
    }
    if (two_sampling_delay == ADC_TWOSAMPLINGDELAY_3CYCLES)
    {
        return 3U;
    }
    if (two_sampling_delay == ADC_TWOSAMPLINGDELAY_4CYCLES)
    {
        return 4U;
    }
    if (two_sampling_delay == ADC_TWOSAMPLINGDELAY_5CYCLES)
    {
        return 5U;
    }
    if (two_sampling_delay == ADC_TWOSAMPLINGDELAY_6CYCLES)
    {
        return 6U;
    }
    if (two_sampling_delay == ADC_TWOSAMPLINGDELAY_7CYCLES)
    {
        return 7U;
    }
    if (two_sampling_delay == ADC_TWOSAMPLINGDELAY_8CYCLES)
    {
        return 8U;
    }
    if (two_sampling_delay == ADC_TWOSAMPLINGDELAY_9CYCLES)
    {
        return 9U;
    }

    return 0U;
}

static uint32_t SCOPE_GetMaxTriggerHzByConfig(const SCOPE_InitTypeDef *cfg)
{
    uint32_t conv_cycles;
    uint32_t pa6_cycles_x2;
    uint32_t ch2_cycles_x2;
    uint32_t tconv_pa6_x2;
    uint32_t tconv_ch2_x2;
    uint32_t tconv_slowest_x2;

    conv_cycles = SCOPE_GetResolutionConvCycles(cfg->resolution);
    pa6_cycles_x2 = SCOPE_GetSampleCyclesX2(cfg->pa6_sampling_time);
    ch2_cycles_x2 = SCOPE_GetSampleCyclesX2(cfg->pc3_sampling_time);

    if ((conv_cycles == 0U) || (pa6_cycles_x2 == 0U) || (ch2_cycles_x2 == 0U))
    {
        return 0U;
    }

    tconv_pa6_x2 = pa6_cycles_x2 + 1U + (2U * conv_cycles);
    tconv_ch2_x2 = ch2_cycles_x2 + 1U + (2U * conv_cycles);
    if (tconv_pa6_x2 >= tconv_ch2_x2)
    {
        tconv_slowest_x2 = tconv_pa6_x2;
    }
    else
    {
        tconv_slowest_x2 = tconv_ch2_x2;
    }

    return (uint32_t)(((uint64_t)cfg->adc_clock_hz * 2ULL) / (uint64_t)tconv_slowest_x2);
}

static HAL_StatusTypeDef SCOPE_ConfigAdcClock(uint32_t adc_clock_hz)
{
    RCC_PeriphCLKInitTypeDef periph_clk_init;

    memset(&periph_clk_init, 0, sizeof(periph_clk_init));

    periph_clk_init.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    periph_clk_init.AdcClockSelection = RCC_ADCCLKSOURCE_PLL2;
    periph_clk_init.PLL2.PLL2M = 5U;
    periph_clk_init.PLL2.PLL2Q = 2U;
    periph_clk_init.PLL2.PLL2R = 2U;
    periph_clk_init.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_2;
    periph_clk_init.PLL2.PLL2VCOSEL = RCC_PLL2VCOWIDE;
    periph_clk_init.PLL2.PLL2FRACN = 0U;

    if (adc_clock_hz <= 36000000U)
    {
        periph_clk_init.PLL2.PLL2N = 144U;
        periph_clk_init.PLL2.PLL2P = 20U; /* 25MHz/5*144/20 = 36MHz */
    }
    else
    {
        periph_clk_init.PLL2.PLL2N = 100U;
        periph_clk_init.PLL2.PLL2P = 10U; /* 25MHz/5*100/10 = 50MHz */
    }

    return HAL_RCCEx_PeriphCLKConfig(&periph_clk_init);
}

static HAL_StatusTypeDef SCOPE_ConfigTriggerTimer(uint32_t trigger_hz)
{
    TIM_MasterConfigTypeDef master_cfg;
    TIM_OC_InitTypeDef oc_cfg;
    uint32_t tim_clk;
    uint32_t total_div;
    uint32_t prescaler;
    uint32_t period;
    uint32_t timer_cnt_clk;
    uint32_t delay_cycles;
    uint32_t midpoint_pulse;

    if (trigger_hz == 0U)
    {
        return HAL_ERROR;
    }

    tim_clk = SCOPE_GetTim8Clock();
    if (tim_clk == 0U)
    {
        return HAL_ERROR;
    }

    total_div = tim_clk / trigger_hz;
    if (total_div == 0U)
    {
        return HAL_ERROR;
    }

    prescaler = total_div / 65536U;
    if (prescaler > 0xFFFFU)
    {
        return HAL_ERROR;
    }

    period = total_div / (prescaler + 1U);
    if (period == 0U)
    {
        return HAL_ERROR;
    }
    period -= 1U;
    if (period < 2U)
    {
        return HAL_ERROR;
    }

    timer_cnt_clk = tim_clk / (prescaler + 1U);
    if (timer_cnt_clk == 0U)
    {
        return HAL_ERROR;
    }

    delay_cycles = SCOPE_GetTwoSamplingDelayCycles(g_scope_cfg.two_sampling_delay);
    if ((delay_cycles == 0U) || (g_scope_cfg.adc_clock_hz == 0U))
    {
        return HAL_ERROR;
    }

    midpoint_pulse = (uint32_t)((((uint64_t)timer_cnt_clk * (uint64_t)delay_cycles) +
                                 (uint64_t)g_scope_cfg.adc_clock_hz) /
                                ((uint64_t)2U * (uint64_t)g_scope_cfg.adc_clock_hz));
    if (midpoint_pulse == 0U)
    {
        midpoint_pulse = 1U;
    }
    if (midpoint_pulse >= period)
    {
        midpoint_pulse = period - 1U;
    }

    hscope_tim8.Instance = TIM8;
    hscope_tim8.Init.Prescaler = prescaler;
    hscope_tim8.Init.CounterMode = TIM_COUNTERMODE_UP;
    hscope_tim8.Init.Period = period;
    hscope_tim8.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    hscope_tim8.Init.RepetitionCounter = 0U;
    hscope_tim8.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    __HAL_RCC_TIM8_CLK_ENABLE();

    if (HAL_TIM_Base_Init(&hscope_tim8) != HAL_OK)
    {
        return HAL_ERROR;
    }

    if (g_scope_cfg.ch2_trigger_mode == SCOPE_CH2_TRIG_MIDPOINT_TRGO2)
    {
        if (HAL_TIM_OC_Init(&hscope_tim8) != HAL_OK)
        {
            return HAL_ERROR;
        }

        memset(&oc_cfg, 0, sizeof(oc_cfg));
        oc_cfg.OCMode = TIM_OCMODE_TIMING;
        oc_cfg.Pulse = midpoint_pulse;
        oc_cfg.OCPolarity = TIM_OCPOLARITY_HIGH;
        oc_cfg.OCFastMode = TIM_OCFAST_DISABLE;
        if (HAL_TIM_OC_ConfigChannel(&hscope_tim8, &oc_cfg, TIM_CHANNEL_1) != HAL_OK)
        {
            return HAL_ERROR;
        }
    }

    memset(&master_cfg, 0, sizeof(master_cfg));
    master_cfg.MasterOutputTrigger = TIM_TRGO_UPDATE;
    if (g_scope_cfg.ch2_trigger_mode == SCOPE_CH2_TRIG_MIDPOINT_TRGO2)
    {
        master_cfg.MasterOutputTrigger2 = TIM_TRGO2_OC1;
    }
    else
    {
        master_cfg.MasterOutputTrigger2 = TIM_TRGO2_RESET;
    }
    master_cfg.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;

    if (HAL_TIMEx_MasterConfigSynchronization(&hscope_tim8, &master_cfg) != HAL_OK)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

static HAL_StatusTypeDef SCOPE_ConfigAdcChannels(const SCOPE_InitTypeDef *cfg)
{
    ADC_ChannelConfTypeDef ch_cfg;

    memset(&ch_cfg, 0, sizeof(ch_cfg));

    ch_cfg.Channel = ADC_CHANNEL_3;
    ch_cfg.Rank = ADC_REGULAR_RANK_1;
    ch_cfg.SamplingTime = cfg->pa6_sampling_time;
    ch_cfg.SingleDiff = ADC_SINGLE_ENDED;
    ch_cfg.OffsetNumber = ADC_OFFSET_NONE;
    ch_cfg.Offset = 0U;
    ch_cfg.OffsetRightShift = DISABLE;
    ch_cfg.OffsetSignedSaturation = DISABLE;
    if (HAL_ADC_ConfigChannel(&hscope_adc1, &ch_cfg) != HAL_OK)
    {
        return HAL_ERROR;
    }

    if (HAL_ADC_ConfigChannel(&hscope_adc2, &ch_cfg) != HAL_OK)
    {
        return HAL_ERROR;
    }

    if (cfg->ch2_input == SCOPE_CH2_INPUT_PC2)
    {
        ch_cfg.Channel = ADC_CHANNEL_0;
    }
    else
    {
        ch_cfg.Channel = ADC_CHANNEL_1;
    }
    ch_cfg.SamplingTime = cfg->pc3_sampling_time;
    if (HAL_ADC_ConfigChannel(&hscope_adc3, &ch_cfg) != HAL_OK)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

static HAL_StatusTypeDef SCOPE_ConfigMultiMode(uint32_t resolution, uint32_t two_sampling_delay)
{
    ADC_MultiModeTypeDef multimode;

    memset(&multimode, 0, sizeof(multimode));
    multimode.Mode = ADC_DUALMODE_INTERL;
    multimode.TwoSamplingDelay = two_sampling_delay;
    if (resolution == ADC_RESOLUTION_8B)
    {
        multimode.DualModeData = ADC_DUALMODEDATAFORMAT_8_BITS;
    }
    else
    {
        multimode.DualModeData = ADC_DUALMODEDATAFORMAT_32_10_BITS;
    }

    return HAL_ADCEx_MultiModeConfigChannel(&hscope_adc1, &multimode);
}

static uint32_t SCOPE_GetTim8Clock(void)
{
    RCC_ClkInitTypeDef clk_init;
    uint32_t flash_latency;
    uint32_t pclk2;

    HAL_RCC_GetClockConfig(&clk_init, &flash_latency);
    pclk2 = HAL_RCC_GetPCLK2Freq();

    if (clk_init.APB2CLKDivider == RCC_HCLK_DIV1)
    {
        return pclk2;
    }

    return (pclk2 * 2U);
}

void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc)
{
    GPIO_InitTypeDef gpio_init;

    if (hadc->Instance == ADC1)
    {
        __HAL_RCC_ADC12_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_DMA1_CLK_ENABLE();

        gpio_init.Pin = GPIO_PIN_6;
        gpio_init.Mode = GPIO_MODE_ANALOG;
        gpio_init.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &gpio_init);

        hscope_dma_adc12.Instance = DMA1_Stream1;
        hscope_dma_adc12.Init.Request = DMA_REQUEST_ADC1;
        hscope_dma_adc12.Init.Direction = DMA_PERIPH_TO_MEMORY;
        hscope_dma_adc12.Init.PeriphInc = DMA_PINC_DISABLE;
        hscope_dma_adc12.Init.MemInc = DMA_MINC_ENABLE;
        hscope_dma_adc12.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
        hscope_dma_adc12.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
        hscope_dma_adc12.Init.Mode = DMA_CIRCULAR;
        hscope_dma_adc12.Init.Priority = DMA_PRIORITY_VERY_HIGH;
        hscope_dma_adc12.Init.FIFOMode = DMA_FIFOMODE_DISABLE;

        HAL_DMA_DeInit(&hscope_dma_adc12);
        HAL_DMA_Init(&hscope_dma_adc12);
        __HAL_LINKDMA(hadc, DMA_Handle, hscope_dma_adc12);

#if SCOPE_USE_DMA_IRQ
        HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, 5U, 0U);
        HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);
#endif
    }
    else if (hadc->Instance == ADC2)
    {
        __HAL_RCC_ADC12_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        gpio_init.Pin = GPIO_PIN_6;
        gpio_init.Mode = GPIO_MODE_ANALOG;
        gpio_init.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &gpio_init);
    }
    else if (hadc->Instance == ADC3)
    {
        __HAL_RCC_ADC3_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();
        __HAL_RCC_DMA2_CLK_ENABLE();

        if (g_scope_cfg.ch2_input == SCOPE_CH2_INPUT_PC2)
        {
            gpio_init.Pin = GPIO_PIN_2;
        }
        else
        {
            gpio_init.Pin = GPIO_PIN_3;
        }
        gpio_init.Mode = GPIO_MODE_ANALOG;
        gpio_init.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOC, &gpio_init);

        hscope_dma_adc3.Instance = DMA2_Stream1;
        hscope_dma_adc3.Init.Request = DMA_REQUEST_ADC3;
        hscope_dma_adc3.Init.Direction = DMA_PERIPH_TO_MEMORY;
        hscope_dma_adc3.Init.PeriphInc = DMA_PINC_DISABLE;
        hscope_dma_adc3.Init.MemInc = DMA_MINC_ENABLE;
        hscope_dma_adc3.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
        hscope_dma_adc3.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
        hscope_dma_adc3.Init.Mode = DMA_CIRCULAR;
        hscope_dma_adc3.Init.Priority = DMA_PRIORITY_VERY_HIGH;
        hscope_dma_adc3.Init.FIFOMode = DMA_FIFOMODE_DISABLE;

        HAL_DMA_DeInit(&hscope_dma_adc3);
        HAL_DMA_Init(&hscope_dma_adc3);
        __HAL_LINKDMA(hadc, DMA_Handle, hscope_dma_adc3);

#if SCOPE_USE_DMA_IRQ
        HAL_NVIC_SetPriority(DMA2_Stream1_IRQn, 5U, 0U);
        HAL_NVIC_EnableIRQ(DMA2_Stream1_IRQn);
#endif
    }
    else
    {
    }
}

void HAL_ADC_MspDeInit(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1)
    {
        if (hscope_dma_adc12.Instance != NULL)
        {
            HAL_DMA_DeInit(&hscope_dma_adc12);
            hscope_dma_adc12.Instance = NULL;
        }
#if SCOPE_USE_DMA_IRQ
        HAL_NVIC_DisableIRQ(DMA1_Stream1_IRQn);
#endif
    }
    else if (hadc->Instance == ADC3)
    {
        if (hscope_dma_adc3.Instance != NULL)
        {
            HAL_DMA_DeInit(&hscope_dma_adc3);
            hscope_dma_adc3.Instance = NULL;
        }
#if SCOPE_USE_DMA_IRQ
        HAL_NVIC_DisableIRQ(DMA2_Stream1_IRQn);
#endif
    }
    else
    {
    }
}

void DMA1_Stream1_IRQHandler(void)
{
#if SCOPE_USE_DMA_IRQ
    HAL_DMA_IRQHandler(&hscope_dma_adc12);
#endif
}

void DMA2_Stream1_IRQHandler(void)
{
#if SCOPE_USE_DMA_IRQ
    HAL_DMA_IRQHandler(&hscope_dma_adc3);
#endif
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
#if SCOPE_USE_DMA_IRQ
    if (hadc->Instance == ADC1)
    {
        SCB_InvalidateDCache_by_Addr((uint32_t *)g_scope_ch1_packed, (int32_t)(sizeof(g_scope_ch1_packed) / 2U));
        g_scope_dma_stats.adc12_half_count++;
#if (SCOPE_SEND_TO_UI_WAVEFORM == 1)
        scope_decimate_ch1(g_scope_ch1_packed, SCOPE_CH1_PACKED_LEN / 2U, s_ui_ch1_half);
        s_ch1_half_ready = 1U;
        scope_send_pair_if_ready(s_ui_ch1_half, s_ui_ch2_half, &s_ch1_half_ready, &s_ch2_half_ready);
#endif
    }
    else if (hadc->Instance == ADC3)
    {
        SCB_InvalidateDCache_by_Addr((uint32_t *)g_scope_ch2_samples, (int32_t)(sizeof(g_scope_ch2_samples) / 2U));
        g_scope_dma_stats.adc3_half_count++;
#if (SCOPE_SEND_TO_UI_WAVEFORM == 1)
        scope_decimate_ch2(g_scope_ch2_samples, SCOPE_CH2_SAMPLES_LEN / 2U, s_ui_ch2_half);
        s_ch2_half_ready = 1U;
        scope_send_pair_if_ready(s_ui_ch1_half, s_ui_ch2_half, &s_ch1_half_ready, &s_ch2_half_ready);
#endif
    }
    else
    {
    }
#else
    (void)hadc;
#endif
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
#if SCOPE_USE_DMA_IRQ
    if (hadc->Instance == ADC1)
    {
        SCB_InvalidateDCache_by_Addr((uint32_t *)&g_scope_ch1_packed[SCOPE_CH1_PACKED_LEN / 2U], (int32_t)(sizeof(g_scope_ch1_packed) / 2U));
        g_scope_dma_stats.adc12_full_count++;
#if (SCOPE_SEND_TO_UI_WAVEFORM == 1)
        scope_decimate_ch1(&g_scope_ch1_packed[SCOPE_CH1_PACKED_LEN / 2U], SCOPE_CH1_PACKED_LEN / 2U, s_ui_ch1_full);
        s_ch1_full_ready = 1U;
        scope_send_pair_if_ready(s_ui_ch1_full, s_ui_ch2_full, &s_ch1_full_ready, &s_ch2_full_ready);
#endif
    }
    else if (hadc->Instance == ADC3)
    {
        SCB_InvalidateDCache_by_Addr((uint32_t *)&g_scope_ch2_samples[SCOPE_CH2_SAMPLES_LEN / 2U], (int32_t)(sizeof(g_scope_ch2_samples) / 2U));
        g_scope_dma_stats.adc3_full_count++;
#if (SCOPE_SEND_TO_UI_WAVEFORM == 1)
        scope_decimate_ch2(&g_scope_ch2_samples[SCOPE_CH2_SAMPLES_LEN / 2U], SCOPE_CH2_SAMPLES_LEN / 2U, s_ui_ch2_full);
        s_ch2_full_ready = 1U;
        scope_send_pair_if_ready(s_ui_ch1_full, s_ui_ch2_full, &s_ch1_full_ready, &s_ch2_full_ready);
#endif
    }
    else
    {
    }
#else
    (void)hadc;
#endif
}

uint32_t SCOPE_GetCh2TriggerMode(void)
{
    return g_scope_cfg.ch2_trigger_mode;
}
