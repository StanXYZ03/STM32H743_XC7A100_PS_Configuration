#ifndef __BSP_ADC_SCOPE_H743_H__
#define __BSP_ADC_SCOPE_H743_H__

#include "stm32h7xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SCOPE_CH1_PACKED_LEN    (16384U)
#define SCOPE_CH2_SAMPLES_LEN   (16384U)

/* 1=在 DMA 回调中解包/抽取后发送到 UI 波形队列；0=不发送 */
#ifndef SCOPE_SEND_TO_UI_WAVEFORM
#define SCOPE_SEND_TO_UI_WAVEFORM  0
#endif

#define SCOPE_CH2_INPUT_PC2     (0U)
#define SCOPE_CH2_INPUT_PC3     (1U)

#define SCOPE_CH2_TRIG_SYNC_TRGO       (0U) /* ADC3 and ADC1 start on same TRGO edge */
#define SCOPE_CH2_TRIG_MIDPOINT_TRGO2  (1U) /* ADC3 midpoint via TIMx TRGO2 */

typedef struct
{
    uint32_t trigger_hz;
    uint32_t adc_clock_hz;
    uint32_t resolution;
    uint32_t pa6_sampling_time;
    uint32_t pc3_sampling_time;
    uint32_t two_sampling_delay;
    uint32_t ch2_input;
    uint32_t ch2_trigger_mode;
} SCOPE_InitTypeDef;

typedef struct
{
    uint32_t adc12_half_count;
    uint32_t adc12_full_count;
    uint32_t adc3_half_count;
    uint32_t adc3_full_count;
} SCOPE_DmaStatsTypeDef;

#define SCOPE_RATECAPS_TYPEDEF_DEFINED 1U

typedef struct
{
    uint32_t max_trigger_hz;
    uint32_t max_ch1_hz;
    uint32_t max_ch2_hz;
} SCOPE_RateCapsTypeDef;

void SCOPE_InitDefaultConfig(SCOPE_InitTypeDef *cfg);
void SCOPE_ApplyResolutionPreset(SCOPE_InitTypeDef *cfg, uint32_t resolution);
HAL_StatusTypeDef SCOPE_GetRateCaps(const SCOPE_InitTypeDef *cfg, SCOPE_RateCapsTypeDef *caps);
HAL_StatusTypeDef SCOPE_Start(const SCOPE_InitTypeDef *cfg);
void SCOPE_Stop(void);
uint8_t SCOPE_IsRunning(void);

void SCOPE_GetBuffers(const uint32_t **ch1_packed,
                      uint32_t *ch1_len,
                      const uint16_t **ch2,
                      uint32_t *ch2_len);

void SCOPE_UnpackCh1(const uint32_t *packed,
                     uint32_t packed_len,
                     uint16_t *out_samples,
                     uint32_t out_len);

void SCOPE_GetDmaStats(SCOPE_DmaStatsTypeDef *stats);
void SCOPE_ClearDmaStats(void);
uint8_t SCOPE_IsDmaProgressing(void);
uint32_t SCOPE_GetCh2TriggerMode(void);

#ifdef __cplusplus
}
#endif

#endif
