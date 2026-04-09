#ifndef APP_SCOPE_CFG_H
#define APP_SCOPE_CFG_H

#include "includes.h"
#include "bsp_adc_scope_h743.h"

typedef enum
{
    APP_SCOPE_STATUS_BOOT = 0,
    APP_SCOPE_STATUS_STARTING,
    APP_SCOPE_STATUS_START_FAIL,
    APP_SCOPE_STATUS_RUNNING,
    APP_SCOPE_STATUS_DMA_STALL
} APP_SCOPE_STATUS_E;

HAL_StatusTypeDef APP_ScopePrepareModeConfig(uint8_t mode_index,
                                             SCOPE_InitTypeDef *cfg,
                                             SCOPE_RateCapsTypeDef *caps);

uint8_t APP_ScopeGetModeCount(void);
uint8_t APP_ScopeGetDefaultModeIndex(void);
const char *APP_ScopeGetModeName(uint8_t mode_index);
uint32_t APP_ScopeGetModeResolution(uint8_t mode_index);
uint16_t APP_ScopeGetModeColor(uint8_t mode_index);
uint32_t APP_ScopeGetResolutionMaxCode(uint32_t resolution);

#endif
