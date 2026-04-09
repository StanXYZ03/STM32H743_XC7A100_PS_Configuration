#ifndef APP_SCOPE_RENDER_H
#define APP_SCOPE_RENDER_H

#include "includes.h"
#include "bsp_adc_scope_h743.h"

void APP_ScopeRenderWaveFrame(uint8_t mode_index, const SCOPE_DmaStatsTypeDef *stats);

#endif
