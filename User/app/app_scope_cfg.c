#include "app_scope_cfg.h"
#include "bsp_lcd_rgb.h"

typedef struct
{
    uint32_t resolution;
    const char *name;
    uint16_t run_color;
} APP_SCOPE_ModeProfileTypeDef;

/* Document profile selector:
 * 0: CH2=PC2, ADC3 synced with ADC1 (TRGO)
 * 1: CH2=PC3, ADC3 synced with ADC1 (TRGO)
 * 2: CH2=PC3, ADC3 midpoint trigger (TRGO2) between ADC1 and ADC2
 */
#define APP_SCOPE_DOC_PROFILE_PC2_SYNC      0U
#define APP_SCOPE_DOC_PROFILE_PC3_SYNC      1U
#define APP_SCOPE_DOC_PROFILE_PC3_MIDPOINT  2U
#define APP_SCOPE_DOC_PROFILE               APP_SCOPE_DOC_PROFILE_PC3_MIDPOINT

static const APP_SCOPE_ModeProfileTypeDef g_scope_modes[] =
{
    {ADC_RESOLUTION_16B, "16-bit", LCD_RGB565(255, 0, 0)},
    {ADC_RESOLUTION_14B, "14-bit", LCD_RGB565(255, 220, 0)},
    {ADC_RESOLUTION_12B, "12-bit", LCD_RGB565(0, 220, 0)},
    {ADC_RESOLUTION_10B, "10-bit", LCD_RGB565(0, 220, 220)},
    {ADC_RESOLUTION_8B,  "8-bit",  LCD_RGB565(0, 80, 255)}
};

#define APP_SCOPE_MODE_COUNT         ((uint8_t)(sizeof(g_scope_modes) / sizeof(g_scope_modes[0])))
#define APP_SCOPE_DEFAULT_MODE_INDEX ((uint8_t)2U)

static uint32_t APP_ScopeSelectRuntimeTriggerHz(const SCOPE_RateCapsTypeDef *caps)
{
    uint32_t trigger_hz;

    if (caps->max_trigger_hz == 0U)
    {
        return 1000000U;
    }

    /* Run close to max capability to match sampling-rate targets in the design doc. */
    trigger_hz = (caps->max_trigger_hz * 9U) / 10U;
    if (trigger_hz == 0U)
    {
        trigger_hz = caps->max_trigger_hz;
    }

    return trigger_hz;
}

static void APP_ScopeApplyDocProfile(SCOPE_InitTypeDef *cfg, uint8_t mode_index)
{
    uint32_t resolution;

    if (cfg == NULL)
    {
        return;
    }

    resolution = g_scope_modes[mode_index].resolution;

#if (APP_SCOPE_DOC_PROFILE == APP_SCOPE_DOC_PROFILE_PC2_SYNC)
    cfg->ch2_input = SCOPE_CH2_INPUT_PC2;
    cfg->ch2_trigger_mode = SCOPE_CH2_TRIG_SYNC_TRGO;
    cfg->pc3_sampling_time = ADC_SAMPLETIME_8CYCLES_5;
#elif (APP_SCOPE_DOC_PROFILE == APP_SCOPE_DOC_PROFILE_PC3_SYNC)
    cfg->ch2_input = SCOPE_CH2_INPUT_PC3;
    cfg->ch2_trigger_mode = SCOPE_CH2_TRIG_SYNC_TRGO;
    if ((resolution == ADC_RESOLUTION_8B) || (resolution == ADC_RESOLUTION_10B))
    {
        cfg->pc3_sampling_time = ADC_SAMPLETIME_2CYCLES_5;
    }
    else
    {
        cfg->pc3_sampling_time = ADC_SAMPLETIME_8CYCLES_5;
    }
#else
    cfg->ch2_input = SCOPE_CH2_INPUT_PC3;
    cfg->ch2_trigger_mode = SCOPE_CH2_TRIG_MIDPOINT_TRGO2;
    if ((resolution == ADC_RESOLUTION_8B) || (resolution == ADC_RESOLUTION_10B))
    {
        cfg->pc3_sampling_time = ADC_SAMPLETIME_2CYCLES_5;
    }
    else
    {
        cfg->pc3_sampling_time = ADC_SAMPLETIME_8CYCLES_5;
    }
#endif
}

HAL_StatusTypeDef APP_ScopePrepareModeConfig(uint8_t mode_index,
                                             SCOPE_InitTypeDef *cfg,
                                             SCOPE_RateCapsTypeDef *caps)
{
    if ((cfg == NULL) || (caps == NULL) || (mode_index >= APP_SCOPE_MODE_COUNT))
    {
        return HAL_ERROR;
    }

    SCOPE_InitDefaultConfig(cfg);
    SCOPE_ApplyResolutionPreset(cfg, g_scope_modes[mode_index].resolution);
    APP_ScopeApplyDocProfile(cfg, mode_index);

    if (SCOPE_GetRateCaps(cfg, caps) != HAL_OK)
    {
        return HAL_ERROR;
    }

    cfg->trigger_hz = APP_ScopeSelectRuntimeTriggerHz(caps);
    return HAL_OK;
}

uint8_t APP_ScopeGetModeCount(void)
{
    return APP_SCOPE_MODE_COUNT;
}

uint8_t APP_ScopeGetDefaultModeIndex(void)
{
    return APP_SCOPE_DEFAULT_MODE_INDEX;
}

const char *APP_ScopeGetModeName(uint8_t mode_index)
{
    if (mode_index < APP_SCOPE_MODE_COUNT)
    {
        return g_scope_modes[mode_index].name;
    }

    return "unknown";
}

uint32_t APP_ScopeGetModeResolution(uint8_t mode_index)
{
    if (mode_index < APP_SCOPE_MODE_COUNT)
    {
        return g_scope_modes[mode_index].resolution;
    }

    return ADC_RESOLUTION_12B;
}

uint16_t APP_ScopeGetModeColor(uint8_t mode_index)
{
    if (mode_index < APP_SCOPE_MODE_COUNT)
    {
        return g_scope_modes[mode_index].run_color;
    }

    return LCD_RGB565(255, 0, 0);
}

uint32_t APP_ScopeGetResolutionMaxCode(uint32_t resolution)
{
    if (resolution == ADC_RESOLUTION_16B)
    {
        return 65535U;
    }
    if (resolution == ADC_RESOLUTION_14B)
    {
        return 16383U;
    }
    if (resolution == ADC_RESOLUTION_12B)
    {
        return 4095U;
    }
    if (resolution == ADC_RESOLUTION_10B)
    {
        return 1023U;
    }
    if (resolution == ADC_RESOLUTION_8B)
    {
        return 255U;
    }

    return 4095U;
}
