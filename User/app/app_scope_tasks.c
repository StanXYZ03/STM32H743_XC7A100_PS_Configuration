#include "includes.h"
#include "bsp_lcd_rgb.h"
#include "bsp_adc_scope_h743.h"
#include "bsp_cpu_dac.h"
#include "app_scope_cfg.h"
#include "app_scope_render.h"
#include "app_scope_tasks.h"

#define ENABLE_SCOPE_TASK_AT_BOOT   1
#define ENABLE_SCOPE_VISUAL_STATUS  1
#define ENABLE_SCOPE_MODE_AUTOSWEEP 1
#define ENABLE_SCOPE_RENDER_TASK    1
#define ENABLE_SCOPE_DAC_OUTPUT     1

#define SCOPE_MODE_SWITCH_PERIOD_MS     5000U
#define SCOPE_STALL_RECOVERY_DELAY_MS    500U
#define SCOPE_RENDER_PERIOD_MS          40U
#define SCOPE_DAC_SINE_VPP             1800U
#define SCOPE_DAC_SINE_FREQ_HZ         2000U

static volatile APP_SCOPE_STATUS_E g_scope_status = APP_SCOPE_STATUS_BOOT;
static volatile uint8_t g_scope_mode_index = 0U;
static uint8_t g_scope_dac_started = 0U;

static TaskHandle_t xHandleTaskLCD = NULL;
static TaskHandle_t xHandleTaskStart = NULL;
#if ENABLE_SCOPE_TASK_AT_BOOT
static TaskHandle_t xHandleTaskScope = NULL;
#if ENABLE_SCOPE_RENDER_TASK
static TaskHandle_t xHandleTaskScopeRender = NULL;
#endif
#endif

static void vTaskLCDColor(void *pvParameters);
static void vTaskStart(void *pvParameters);
static void APP_ScopeEnsureDacStarted(void);
#if ENABLE_SCOPE_TASK_AT_BOOT
static void vTaskScope(void *pvParameters);
#if ENABLE_SCOPE_RENDER_TASK
static void vTaskScopeRender(void *pvParameters);
#endif
#endif

static void APP_ScopeEnsureDacStarted(void)
{
#if ENABLE_SCOPE_DAC_OUTPUT
    if (g_scope_dac_started == 0U)
    {
        dac1_SetSinWave((uint16_t)SCOPE_DAC_SINE_VPP, SCOPE_DAC_SINE_FREQ_HZ);
        g_scope_dac_started = 1U;
        BSP_Printf("DAC1 start: PA4 sine, freq=%luHz, vpp=%lu\r\n",
                   (unsigned long)SCOPE_DAC_SINE_FREQ_HZ,
                   (unsigned long)SCOPE_DAC_SINE_VPP);
    }
#endif
}

static void vTaskLCDColor(void *pvParameters)
{
#if ENABLE_SCOPE_VISUAL_STATUS
    APP_SCOPE_STATUS_E last_status;
    APP_SCOPE_STATUS_E status;
    uint8_t mode_index;
    uint8_t last_mode_index;
    uint8_t blink;

    (void)pvParameters;

    last_status = (APP_SCOPE_STATUS_E)0xFFU;
    last_mode_index = 0xFFU;
    blink = 0U;

    while (1)
    {
        status = g_scope_status;
        mode_index = g_scope_mode_index;

        if (status == APP_SCOPE_STATUS_RUNNING)
        {
#if ENABLE_SCOPE_RENDER_TASK
            if ((last_status != status) || (last_mode_index != mode_index))
            {
                /* keep state edge for color task bookkeeping */
            }
            vTaskDelay(pdMS_TO_TICKS(100));
#else
            if ((last_status != status) || (last_mode_index != mode_index))
            {
                LCD_RGB_Fill(APP_ScopeGetModeColor(mode_index));
            }
            vTaskDelay(pdMS_TO_TICKS(100));
#endif
        }
        else if (status == APP_SCOPE_STATUS_STARTING)
        {
            blink ^= 1U;
            LCD_RGB_Fill(blink ? LCD_RGB565(0, 80, 255) : LCD_RGB565(0, 0, 0));
            vTaskDelay(pdMS_TO_TICKS(150));
        }
        else if (status == APP_SCOPE_STATUS_START_FAIL)
        {
            blink ^= 1U;
            LCD_RGB_Fill(blink ? LCD_RGB565(255, 0, 255) : LCD_RGB565(0, 0, 0));
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        else if (status == APP_SCOPE_STATUS_DMA_STALL)
        {
            blink ^= 1U;
            LCD_RGB_Fill(blink ? LCD_RGB565(255, 128, 0) : LCD_RGB565(0, 0, 0));
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        else
        {
            if (last_status != status)
            {
                LCD_RGB_Fill(LCD_RGB565(255, 255, 255));
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        last_status = status;
        last_mode_index = mode_index;
    }
#else
    static const uint16_t colors[] = {
        LCD_RGB565(255, 255, 255),
        LCD_RGB565(0, 0, 0),
        LCD_RGB565(0, 80, 255),
        LCD_RGB565(255, 0, 0),
        LCD_RGB565(255, 0, 255),
        LCD_RGB565(0, 220, 0),
        LCD_RGB565(0, 220, 220),
        LCD_RGB565(255, 215, 0),
        LCD_RGB565(255, 90, 60),
        LCD_RGB565(128, 128, 128),
        LCD_RGB565(200, 200, 200),
        LCD_RGB565(139, 69, 19)
    };
    uint32_t i;

    (void)pvParameters;

    while (1)
    {
        for (i = 0U; i < (sizeof(colors) / sizeof(colors[0])); i++)
        {
            LCD_RGB_Fill(colors[i]);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
#endif
}

static void vTaskStart(void *pvParameters)
{
    TickType_t xLastWakeTime;

    (void)pvParameters;

    HAL_ResumeTick();

    xLastWakeTime = xTaskGetTickCount();

    while (1)
    {
        bsp_ProPer1ms();
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1));
    }
}

#if ENABLE_SCOPE_TASK_AT_BOOT
static void vTaskScope(void *pvParameters)
{
    SCOPE_InitTypeDef cfg;
    SCOPE_RateCapsTypeDef caps;
    TickType_t mode_start_tick;
    uint8_t mode_index;
    uint8_t stall_count;

    (void)pvParameters;

    mode_index = APP_ScopeGetDefaultModeIndex();
    g_scope_mode_index = mode_index;

    vTaskDelay(pdMS_TO_TICKS(100));
    APP_ScopeEnsureDacStarted();

    while (1)
    {
        g_scope_status = APP_SCOPE_STATUS_STARTING;

        if (APP_ScopePrepareModeConfig(mode_index, &cfg, &caps) != HAL_OK)
        {
            BSP_Printf("SCOPE cfg invalid, mode=%s\r\n", APP_ScopeGetModeName(mode_index));
            g_scope_status = APP_SCOPE_STATUS_START_FAIL;
            vTaskDelay(pdMS_TO_TICKS(500));
#if ENABLE_SCOPE_MODE_AUTOSWEEP
            mode_index = (uint8_t)((mode_index + 1U) % APP_ScopeGetModeCount());
#endif
            continue;
        }

        if (SCOPE_Start(&cfg) != HAL_OK)
        {
            BSP_Printf("SCOPE_Start failed, mode=%s\r\n", APP_ScopeGetModeName(mode_index));
            g_scope_status = APP_SCOPE_STATUS_START_FAIL;
            vTaskDelay(pdMS_TO_TICKS(500));
#if ENABLE_SCOPE_MODE_AUTOSWEEP
            mode_index = (uint8_t)((mode_index + 1U) % APP_ScopeGetModeCount());
#endif
            continue;
        }

        g_scope_mode_index = mode_index;
        g_scope_status = APP_SCOPE_STATUS_RUNNING;
        BSP_Printf("SCOPE mode=%s trig=%lu cap=%lu CH1=%lu CH2=%lu\r\n",
                   APP_ScopeGetModeName(mode_index),
                   cfg.trigger_hz,
                   caps.max_trigger_hz,
                   caps.max_ch1_hz,
                   caps.max_ch2_hz);
        BSP_Printf("SCOPE cfg: CH2=%s trig=%s tsamp_pc3=%lu delay=%lu adcclk=%lu\r\n",
                   (cfg.ch2_input == SCOPE_CH2_INPUT_PC2) ? "PC2" : "PC3",
                   (cfg.ch2_trigger_mode == SCOPE_CH2_TRIG_SYNC_TRGO) ? "TRGO-sync" : "TRGO2-mid",
                   cfg.pc3_sampling_time,
                   cfg.two_sampling_delay,
                   cfg.adc_clock_hz);

        stall_count = 0U;
        mode_start_tick = xTaskGetTickCount();

        while (1)
        {
            vTaskDelay(pdMS_TO_TICKS(200));

            if (SCOPE_IsDmaProgressing() != 0U)
            {
                g_scope_status = APP_SCOPE_STATUS_RUNNING;
                stall_count = 0U;
            }
            else
            {
                if (stall_count < 255U)
                {
                    stall_count++;
                }
                if (stall_count >= 3U)
                {
                    g_scope_status = APP_SCOPE_STATUS_DMA_STALL;
                    BSP_Printf("SCOPE DMA stall, mode=%s\r\n", APP_ScopeGetModeName(mode_index));
                    break;
                }
            }

#if ENABLE_SCOPE_MODE_AUTOSWEEP
            if ((xTaskGetTickCount() - mode_start_tick) >= pdMS_TO_TICKS(SCOPE_MODE_SWITCH_PERIOD_MS))
            {
                mode_index = (uint8_t)((mode_index + 1U) % APP_ScopeGetModeCount());
                break;
            }
#endif
        }

        SCOPE_Stop();

        if (g_scope_status == APP_SCOPE_STATUS_DMA_STALL)
        {
            vTaskDelay(pdMS_TO_TICKS(SCOPE_STALL_RECOVERY_DELAY_MS));
        }
    }
}
#endif

#if ENABLE_SCOPE_TASK_AT_BOOT && ENABLE_SCOPE_RENDER_TASK
static void vTaskScopeRender(void *pvParameters)
{
    TickType_t xLastWakeTime;
    SCOPE_DmaStatsTypeDef dma_stats;

    (void)pvParameters;

    memset(&dma_stats, 0, sizeof(dma_stats));
    xLastWakeTime = xTaskGetTickCount();

    while (1)
    {
        if ((g_scope_status == APP_SCOPE_STATUS_RUNNING) && (SCOPE_IsRunning() != 0U))
        {
            SCOPE_GetDmaStats(&dma_stats);
            APP_ScopeRenderWaveFrame(g_scope_mode_index, &dma_stats);
            vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(SCOPE_RENDER_PERIOD_MS));
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(100));
            xLastWakeTime = xTaskGetTickCount();
        }
    }
}
#endif

void APP_CreateTasks(void)
{
    if (xTaskCreate(vTaskLCDColor, "vTaskLCD", 512, NULL, 2, &xHandleTaskLCD) != pdPASS)
    {
        Error_Handler(__FILE__, __LINE__);
    }

    if (xTaskCreate(vTaskStart, "vTaskStart", 512, NULL, 4, &xHandleTaskStart) != pdPASS)
    {
        Error_Handler(__FILE__, __LINE__);
    }

#if ENABLE_SCOPE_TASK_AT_BOOT
    if (xTaskCreate(vTaskScope, "vTaskScope", 2048, NULL, 1, &xHandleTaskScope) != pdPASS)
    {
        Error_Handler(__FILE__, __LINE__);
    }
#if ENABLE_SCOPE_RENDER_TASK
    if (xTaskCreate(vTaskScopeRender, "vTaskScopeR", 2048, NULL, 2, &xHandleTaskScopeRender) != pdPASS)
    {
        Error_Handler(__FILE__, __LINE__);
    }
#endif
#endif
}
