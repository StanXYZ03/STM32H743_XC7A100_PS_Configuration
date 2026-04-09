#include "app_scope_render.h"
#include "bsp_lcd_rgb.h"
#include "app_scope_cfg.h"

#define APP_SCOPE_FB_ROTATE_90CW 1U

static uint16_t APP_ScopeUIGetWidth(void);
static uint16_t APP_ScopeUIGetHeight(void);
static void APP_ScopeFBPutPixel(uint16_t x_ui, uint16_t y_ui, uint16_t color);
static void APP_ScopeFBFillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
static void APP_ScopeFBDrawVSegment(uint16_t x, uint16_t y1, uint16_t y2, uint16_t color);
static void APP_ScopeGetSamplesAtX(uint16_t x,
                                   uint16_t plot_w,
                                   uint32_t total_ch1_samples,
                                   uint32_t ch1_pairs,
                                   uint32_t ch2_count,
                                   uint32_t ch2_trigger_mode,
                                   const uint32_t *ch1_view,
                                   const uint16_t *ch2_view,
                                   uint32_t *ch1_sample,
                                   uint32_t *ch2_sample);

static uint16_t APP_ScopeUIGetWidth(void)
{
#if (APP_SCOPE_FB_ROTATE_90CW == 1U)
    return (uint16_t)LCD_RGB_HEIGHT;
#else
    return (uint16_t)LCD_RGB_WIDTH;
#endif
}

static uint16_t APP_ScopeUIGetHeight(void)
{
#if (APP_SCOPE_FB_ROTATE_90CW == 1U)
    return (uint16_t)LCD_RGB_WIDTH;
#else
    return (uint16_t)LCD_RGB_HEIGHT;
#endif
}

static void APP_ScopeFBPutPixel(uint16_t x_ui, uint16_t y_ui, uint16_t color)
{
    uint16_t *fb_base;
    uint32_t phys_x;
    uint32_t phys_y;

    fb_base = (uint16_t *)LCD_RGB_FB_ADDR;

#if (APP_SCOPE_FB_ROTATE_90CW == 1U)
    if ((x_ui >= (uint16_t)LCD_RGB_HEIGHT) || (y_ui >= (uint16_t)LCD_RGB_WIDTH))
    {
        return;
    }

    phys_x = (uint32_t)y_ui;
    phys_y = (uint32_t)((uint16_t)LCD_RGB_HEIGHT - 1U - x_ui);
#else
    if ((x_ui >= (uint16_t)LCD_RGB_WIDTH) || (y_ui >= (uint16_t)LCD_RGB_HEIGHT))
    {
        return;
    }

    phys_x = (uint32_t)x_ui;
    phys_y = (uint32_t)y_ui;
#endif

    fb_base[phys_y * (uint32_t)LCD_RGB_WIDTH + phys_x] = color;
}

static void APP_ScopeFBFillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    uint32_t yy;
    uint32_t xx;
    uint32_t ui_w;
    uint32_t ui_h;

    if ((w == 0U) || (h == 0U))
    {
        return;
    }

    ui_w = APP_ScopeUIGetWidth();
    ui_h = APP_ScopeUIGetHeight();
    if ((x >= ui_w) || (y >= ui_h))
    {
        return;
    }
    if ((uint32_t)x + (uint32_t)w > ui_w)
    {
        w = (uint16_t)(ui_w - x);
    }
    if ((uint32_t)y + (uint32_t)h > ui_h)
    {
        h = (uint16_t)(ui_h - y);
    }

    for (yy = 0U; yy < h; yy++)
    {
        for (xx = 0U; xx < w; xx++)
        {
            APP_ScopeFBPutPixel((uint16_t)(x + xx), (uint16_t)(y + yy), color);
        }
    }
}

static void APP_ScopeFBDrawVSegment(uint16_t x, uint16_t y1, uint16_t y2, uint16_t color)
{
    uint16_t y_min;
    uint16_t y_max;
    uint32_t ui_w;
    uint32_t ui_h;
    uint32_t y;

    ui_w = APP_ScopeUIGetWidth();
    ui_h = APP_ScopeUIGetHeight();
    if (x >= ui_w)
    {
        return;
    }

    if (y1 <= y2)
    {
        y_min = y1;
        y_max = y2;
    }
    else
    {
        y_min = y2;
        y_max = y1;
    }

    if (y_min >= ui_h)
    {
        return;
    }
    if (y_max >= ui_h)
    {
        y_max = (uint16_t)(ui_h - 1U);
    }

    for (y = y_min; y <= y_max; y++)
    {
        APP_ScopeFBPutPixel(x, (uint16_t)y, color);
    }
}

static void APP_ScopeGetSamplesAtX(uint16_t x,
                                   uint16_t plot_w,
                                   uint32_t total_ch1_samples,
                                   uint32_t ch1_pairs,
                                   uint32_t ch2_count,
                                   uint32_t ch2_trigger_mode,
                                   const uint32_t *ch1_view,
                                   const uint16_t *ch2_view,
                                   uint32_t *ch1_sample,
                                   uint32_t *ch2_sample)
{
    uint64_t idx64;
    uint32_t idx_ch1;
    uint32_t pair_idx;
    uint32_t ch2_idx;

    if ((ch1_sample == NULL) || (ch2_sample == NULL) || (plot_w == 0U))
    {
        return;
    }

    if ((plot_w <= 1U) || (total_ch1_samples <= 1U))
    {
        idx_ch1 = 0U;
    }
    else
    {
        idx64 = ((uint64_t)x * (uint64_t)(total_ch1_samples - 1U)) / (uint64_t)(plot_w - 1U);
        idx_ch1 = (uint32_t)idx64;
    }

    pair_idx = idx_ch1 / 2U;
    if (pair_idx >= ch1_pairs)
    {
        pair_idx = ch1_pairs - 1U;
    }

    if ((idx_ch1 & 1U) == 0U)
    {
        *ch1_sample = (uint16_t)(ch1_view[pair_idx] & 0xFFFFU);
    }
    else
    {
        *ch1_sample = (uint16_t)((ch1_view[pair_idx] >> 16U) & 0xFFFFU);
    }

    ch2_idx = pair_idx;
    if (ch2_idx >= ch2_count)
    {
        ch2_idx = ch2_count - 1U;
    }

    if (ch2_count <= 1U)
    {
        *ch2_sample = ch2_view[0];
        return;
    }

    if (ch2_trigger_mode == SCOPE_CH2_TRIG_SYNC_TRGO)
    {
        if ((idx_ch1 & 1U) == 0U)
        {
            *ch2_sample = ch2_view[ch2_idx];
        }
        else
        {
            uint32_t next_idx = ch2_idx + 1U;
            if (next_idx >= ch2_count)
            {
                *ch2_sample = ch2_view[ch2_idx];
            }
            else
            {
                *ch2_sample = ((uint32_t)ch2_view[ch2_idx] + (uint32_t)ch2_view[next_idx]) / 2U;
            }
        }
    }
    else
    {
        if ((idx_ch1 & 1U) != 0U)
        {
            *ch2_sample = ch2_view[ch2_idx];
        }
        else if (ch2_idx == 0U)
        {
            *ch2_sample = ch2_view[0];
        }
        else
        {
            *ch2_sample = ((uint32_t)ch2_view[ch2_idx - 1U] + (uint32_t)ch2_view[ch2_idx]) / 2U;
        }
    }
}

void APP_ScopeRenderWaveFrame(uint8_t mode_index, const SCOPE_DmaStatsTypeDef *stats)
{
    const uint32_t *ch1_packed;
    const uint16_t *ch2_samples;
    const uint32_t *ch1_view;
    const uint16_t *ch2_view;
    uint16_t plot_x;
    uint16_t plot_y;
    uint16_t plot_w;
    uint16_t plot_h;
    uint16_t x;
    uint16_t prev_y1;
    uint16_t prev_y2;
    uint8_t has_prev;
    uint32_t ch1_len;
    uint32_t ch2_len;
    uint32_t ch1_pairs;
    uint32_t ch2_count;
    uint32_t ch2_trigger_mode;
    uint32_t full_scale;
    uint32_t half_select;
    uint32_t total_ch1_samples;
    uint32_t mean_ch1;
    uint32_t mean_ch2;
    uint32_t max_dev_ch1;
    uint32_t max_dev_ch2;
    uint32_t flat_threshold;
    uint64_t sum_ch1;
    uint64_t sum_ch2;
    uint16_t ui_w;
    uint16_t ui_h;
    uint16_t ch1_center;
    uint16_t ch2_center;
    uint16_t lane_half;
    uint16_t ch1_min;
    uint16_t ch1_max;
    uint16_t ch2_min;
    uint16_t ch2_max;
    uint8_t ch1_flat;
    uint8_t ch2_flat;
    const uint16_t color_bg = LCD_RGB565(5, 8, 16);
    const uint16_t color_grid = LCD_RGB565(28, 34, 44);
    const uint16_t color_axis = LCD_RGB565(60, 70, 90);
    const uint16_t color_ch1 = LCD_RGB565(255, 230, 40);
    const uint16_t color_ch2 = LCD_RGB565(40, 220, 255);

    if (mode_index >= APP_ScopeGetModeCount())
    {
        mode_index = APP_ScopeGetDefaultModeIndex();
    }

    if (stats == NULL)
    {
        return;
    }

    ui_w = APP_ScopeUIGetWidth();
    ui_h = APP_ScopeUIGetHeight();

    plot_x = 0U;
    plot_y = (ui_h > 120U) ? 24U : 8U;
    plot_w = ui_w;
    plot_h = (uint16_t)(ui_h - (uint16_t)(plot_y * 2U));
    if (plot_h < 80U)
    {
        return;
    }

    APP_ScopeFBFillRect(0U, 0U, ui_w, ui_h, color_bg);

    for (x = 0U; x < plot_w; x = (uint16_t)(x + 60U))
    {
        APP_ScopeFBDrawVSegment((uint16_t)(plot_x + x), plot_y, (uint16_t)(plot_y + plot_h - 1U), color_grid);
    }

    for (x = 0U; x < plot_h; x = (uint16_t)(x + 60U))
    {
        APP_ScopeFBFillRect(plot_x, (uint16_t)(plot_y + x), plot_w, 1U, color_grid);
    }

    APP_ScopeFBFillRect(plot_x, (uint16_t)(plot_y + (plot_h / 2U)), plot_w, 1U, color_axis);

    SCOPE_GetBuffers(&ch1_packed, &ch1_len, &ch2_samples, &ch2_len);

    if ((ch1_len < 4U) || (ch2_len < 4U))
    {
        return;
    }

    ch1_pairs = ch1_len / 2U;
    ch2_count = ch2_len / 2U;
    if ((ch1_pairs < 2U) || (ch2_count < 2U))
    {
        return;
    }

    half_select = (stats->adc12_half_count > stats->adc12_full_count) ? 0U : 1U;
    ch1_view = &ch1_packed[half_select * ch1_pairs];
    ch2_view = &ch2_samples[half_select * ch2_count];
    total_ch1_samples = ch1_pairs * 2U;
    ch2_trigger_mode = SCOPE_GetCh2TriggerMode();

    full_scale = APP_ScopeGetResolutionMaxCode(APP_ScopeGetModeResolution(mode_index));
    if (full_scale == 0U)
    {
        full_scale = 4095U;
    }

    ch1_center = (uint16_t)(plot_y + ((uint32_t)plot_h * 2U) / 5U);
    ch2_center = (uint16_t)(plot_y + ((uint32_t)plot_h * 3U) / 5U);
    lane_half = (uint16_t)(plot_h / 7U);
    if (lane_half < 8U)
    {
        lane_half = 8U;
    }
    if ((uint32_t)lane_half * 2U >= plot_h)
    {
        lane_half = (uint16_t)((plot_h > 4U) ? ((plot_h / 2U) - 2U) : 1U);
    }

    ch1_min = (uint16_t)(ch1_center - lane_half);
    ch1_max = (uint16_t)(ch1_center + lane_half);
    ch2_min = (uint16_t)(ch2_center - lane_half);
    ch2_max = (uint16_t)(ch2_center + lane_half);

    APP_ScopeFBFillRect(plot_x, ch1_center, plot_w, 1U, color_axis);
    APP_ScopeFBFillRect(plot_x, ch2_center, plot_w, 1U, color_axis);

    sum_ch1 = 0U;
    sum_ch2 = 0U;
    for (x = 0U; x < plot_w; x++)
    {
        uint32_t ch1_sample;
        uint32_t ch2_sample;

        APP_ScopeGetSamplesAtX(x, plot_w, total_ch1_samples, ch1_pairs, ch2_count, ch2_trigger_mode,
                               ch1_view, ch2_view, &ch1_sample, &ch2_sample);
        if (ch1_sample > full_scale)
        {
            ch1_sample = full_scale;
        }
        if (ch2_sample > full_scale)
        {
            ch2_sample = full_scale;
        }

        sum_ch1 += ch1_sample;
        sum_ch2 += ch2_sample;
    }

    mean_ch1 = (uint32_t)(sum_ch1 / (uint64_t)plot_w);
    mean_ch2 = (uint32_t)(sum_ch2 / (uint64_t)plot_w);

    max_dev_ch1 = 0U;
    max_dev_ch2 = 0U;
    for (x = 0U; x < plot_w; x++)
    {
        uint32_t ch1_sample;
        uint32_t ch2_sample;
        uint32_t dev1;
        uint32_t dev2;

        APP_ScopeGetSamplesAtX(x, plot_w, total_ch1_samples, ch1_pairs, ch2_count, ch2_trigger_mode,
                               ch1_view, ch2_view, &ch1_sample, &ch2_sample);
        if (ch1_sample > full_scale)
        {
            ch1_sample = full_scale;
        }
        if (ch2_sample > full_scale)
        {
            ch2_sample = full_scale;
        }

        dev1 = (ch1_sample > mean_ch1) ? (ch1_sample - mean_ch1) : (mean_ch1 - ch1_sample);
        dev2 = (ch2_sample > mean_ch2) ? (ch2_sample - mean_ch2) : (mean_ch2 - ch2_sample);
        if (dev1 > max_dev_ch1)
        {
            max_dev_ch1 = dev1;
        }
        if (dev2 > max_dev_ch2)
        {
            max_dev_ch2 = dev2;
        }
    }

    flat_threshold = full_scale / 512U;
    if (flat_threshold < 2U)
    {
        flat_threshold = 2U;
    }
    ch1_flat = (max_dev_ch1 <= flat_threshold) ? 1U : 0U;
    ch2_flat = (max_dev_ch2 <= flat_threshold) ? 1U : 0U;

    if (max_dev_ch1 == 0U)
    {
        max_dev_ch1 = 1U;
    }
    if (max_dev_ch2 == 0U)
    {
        max_dev_ch2 = 1U;
    }

    has_prev = 0U;
    prev_y1 = 0U;
    prev_y2 = 0U;

    for (x = 0U; x < plot_w; x++)
    {
        uint32_t ch1_sample;
        uint32_t ch2_sample;
        int32_t d1;
        int32_t d2;
        int32_t y1s;
        int32_t y2s;
        uint16_t y1;
        uint16_t y2;

        APP_ScopeGetSamplesAtX(x, plot_w, total_ch1_samples, ch1_pairs, ch2_count, ch2_trigger_mode,
                               ch1_view, ch2_view, &ch1_sample, &ch2_sample);

        if (ch1_sample > full_scale)
        {
            ch1_sample = full_scale;
        }
        if (ch2_sample > full_scale)
        {
            ch2_sample = full_scale;
        }

        if (ch1_flat != 0U)
        {
            y1 = ch1_center;
        }
        else
        {
            d1 = (int32_t)ch1_sample - (int32_t)mean_ch1;
            y1s = (int32_t)ch1_center - (int32_t)(((int64_t)d1 * (int64_t)lane_half) / (int64_t)max_dev_ch1);
            if (y1s < (int32_t)ch1_min)
            {
                y1s = (int32_t)ch1_min;
            }
            else if (y1s > (int32_t)ch1_max)
            {
                y1s = (int32_t)ch1_max;
            }
            y1 = (uint16_t)y1s;
        }

        if (ch2_flat != 0U)
        {
            y2 = ch2_center;
        }
        else
        {
            d2 = (int32_t)ch2_sample - (int32_t)mean_ch2;
            y2s = (int32_t)ch2_center - (int32_t)(((int64_t)d2 * (int64_t)lane_half) / (int64_t)max_dev_ch2);
            if (y2s < (int32_t)ch2_min)
            {
                y2s = (int32_t)ch2_min;
            }
            else if (y2s > (int32_t)ch2_max)
            {
                y2s = (int32_t)ch2_max;
            }
            y2 = (uint16_t)y2s;
        }

        if (has_prev == 0U)
        {
            APP_ScopeFBDrawVSegment((uint16_t)(plot_x + x), y1, y1, color_ch1);
            APP_ScopeFBDrawVSegment((uint16_t)(plot_x + x), y2, y2, color_ch2);
            has_prev = 1U;
        }
        else
        {
            int32_t dy1 = (int32_t)y1 - (int32_t)prev_y1;
            int32_t dy2 = (int32_t)y2 - (int32_t)prev_y2;

            if (dy1 < 0)
            {
                dy1 = -dy1;
            }
            if (dy2 < 0)
            {
                dy2 = -dy2;
            }

            if ((uint32_t)dy1 <= (uint32_t)(lane_half / 2U))
            {
                APP_ScopeFBDrawVSegment((uint16_t)(plot_x + x), prev_y1, y1, color_ch1);
            }
            else
            {
                APP_ScopeFBDrawVSegment((uint16_t)(plot_x + x), y1, y1, color_ch1);
            }

            if ((uint32_t)dy2 <= (uint32_t)(lane_half / 2U))
            {
                APP_ScopeFBDrawVSegment((uint16_t)(plot_x + x), prev_y2, y2, color_ch2);
            }
            else
            {
                APP_ScopeFBDrawVSegment((uint16_t)(plot_x + x), y2, y2, color_ch2);
            }
        }

        prev_y1 = y1;
        prev_y2 = y2;
    }
}
