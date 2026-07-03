/**
  ******************************************************************************
  * @file    lcd_ltdc.c
  * @brief   Thin framebuffer drawing helpers for the SPI monitor page
  ******************************************************************************
  */

#include "lcd_ltdc.h"
#include "bsp_lcd_rgb.h"
#include "lcd_font.h"

static uint32_t g_back_color = LCD_COLOR_BLACK;
static uint32_t g_text_color = LCD_COLOR_WHITE;

#define LCD_DCACHE_LINE_SIZE    32U

static uint16_t *LCD_GetFrameBuffer(void)
{
    return (uint16_t *)LCD_RGB_FB_ADDR;
}

static void LCD_CleanDCacheRegion(const void *addr, uint32_t size)
{
    uintptr_t start;
    uintptr_t aligned_start;
    uintptr_t aligned_end;

    if (size == 0U)
    {
        return;
    }

    start = (uintptr_t)addr;
    aligned_start = start & ~((uintptr_t)LCD_DCACHE_LINE_SIZE - 1U);
    aligned_end = (start + (uintptr_t)size + (uintptr_t)LCD_DCACHE_LINE_SIZE - 1U) &
                  ~((uintptr_t)LCD_DCACHE_LINE_SIZE - 1U);

    SCB_CleanDCache_by_Addr((uint32_t *)aligned_start, (int32_t)(aligned_end - aligned_start));
}

static void LCD_MapLogicalToPhysical(uint16_t x, uint16_t y, uint16_t *phys_x, uint16_t *phys_y)
{
    *phys_x = y;
    *phys_y = (uint16_t)(LCD_WIDTH - 1U - x);
}

static uint16_t RGB888_to_RGB565(uint32_t color)
{
    uint16_t r = (uint16_t)(((color >> 16) & 0xFFU) >> 3);
    uint16_t g = (uint16_t)(((color >> 8) & 0xFFU) >> 2);
    uint16_t b = (uint16_t)((color & 0xFFU) >> 3);
    return (uint16_t)((r << 11) | (g << 5) | b);
}

static void LCD_DrawSquareGlyphColumnLSB(uint16_t x, uint16_t y, const uint8_t *glyph, uint8_t size,
                                         uint32_t fc, uint32_t bc)
{
    uint8_t bytes_per_column = (uint8_t)(size / 8U + ((size % 8U) ? 1U : 0U));
    uint16_t col;
    uint16_t row;
    uint8_t byte_idx;
    uint8_t bit;
    uint8_t temp;

    for (col = 0U; col < size; col++)
    {
        for (byte_idx = 0U; byte_idx < bytes_per_column; byte_idx++)
        {
            temp = glyph[(uint16_t)(col * bytes_per_column + byte_idx)];
            for (bit = 0U; bit < 8U; bit++)
            {
                row = (uint16_t)((uint16_t)byte_idx * 8U + bit);
                if (row >= size)
                {
                    break;
                }

                LCD_DrawPoint((uint16_t)(x + col), (uint16_t)(y + row),
                              (temp & (uint8_t)(0x01U << bit)) ? fc : bc);
            }
        }
    }
}

static const uint8_t *LCD_FindChineseGlyph(const uint8_t *index, uint8_t size)
{
    uint16_t i;

    switch (size)
    {
        case 12:
            for (i = 0U; i < (uint16_t)(sizeof(tfont12) / sizeof(tfont12[0])); i++)
            {
                if ((tfont12[i].Index[0] == index[0]) && (tfont12[i].Index[1] == index[1]))
                {
                    return tfont12[i].Msk;
                }
            }
            break;

        case 16:
            for (i = 0U; i < (uint16_t)(sizeof(tfont16) / sizeof(tfont16[0])); i++)
            {
                if ((tfont16[i].Index[0] == index[0]) && (tfont16[i].Index[1] == index[1]))
                {
                    return tfont16[i].Msk;
                }
            }
            break;

        case 24:
            for (i = 0U; i < (uint16_t)(sizeof(tfont24) / sizeof(tfont24[0])); i++)
            {
                if ((tfont24[i].Index[0] == index[0]) && (tfont24[i].Index[1] == index[1]))
                {
                    return tfont24[i].Msk;
                }
            }
            break;

        case 32:
            for (i = 0U; i < (uint16_t)(sizeof(tfont32) / sizeof(tfont32[0])); i++)
            {
                if ((tfont32[i].Index[0] == index[0]) && (tfont32[i].Index[1] == index[1]))
                {
                    return tfont32[i].Msk;
                }
            }
            break;

        default:
            break;
    }

    return NULL;
}

void LCD_Clear(uint32_t color)
{
    uint16_t *buffer = LCD_GetFrameBuffer();
    uint32_t total_pixels = LCD_PHYS_WIDTH * LCD_PHYS_HEIGHT;
    uint16_t rgb565 = RGB888_to_RGB565(color);
    uint32_t i;

    for (i = 0U; i < total_pixels; i++)
    {
        buffer[i] = rgb565;
    }

    LCD_CleanDCacheRegion(buffer, total_pixels * sizeof(uint16_t));
}

void LCD_DrawPoint(uint16_t x, uint16_t y, uint32_t color)
{
    uint16_t phys_x;
    uint16_t phys_y;
    uint16_t *buffer;

    if ((x >= LCD_WIDTH) || (y >= LCD_HEIGHT))
    {
        return;
    }

    LCD_MapLogicalToPhysical(x, y, &phys_x, &phys_y);
    buffer = LCD_GetFrameBuffer();
    buffer[(uint32_t)phys_y * LCD_PHYS_WIDTH + phys_x] = RGB888_to_RGB565(color);
}

void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint32_t color)
{
    int dx = (x2 > x1) ? (int)(x2 - x1) : (int)(x1 - x2);
    int dy = (y2 > y1) ? (int)(y2 - y1) : (int)(y1 - y2);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    while (1)
    {
        LCD_DrawPoint(x1, y1, color);
        if ((x1 == x2) && (y1 == y2))
        {
            break;
        }

        if ((2 * err) > -dy)
        {
            err -= dy;
            x1 = (uint16_t)((int)x1 + sx);
        }
        if ((2 * err) < dx)
        {
            err += dx;
            y1 = (uint16_t)((int)y1 + sy);
        }
    }
}

void LCD_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint32_t color)
{
    LCD_DrawLine(x1, y1, x2, y1, color);
    LCD_DrawLine(x1, y1, x1, y2, color);
    LCD_DrawLine(x2, y1, x2, y2, color);
    LCD_DrawLine(x1, y2, x2, y2, color);
}

void LCD_Fill(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint32_t color)
{
    uint16_t x;
    uint16_t y;

    for (y = y1; y <= y2; y++)
    {
        for (x = x1; x <= x2; x++)
        {
            LCD_DrawPoint(x, y, color);
        }
    }
}

void LCD_FillRounded(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t r, uint32_t color)
{
    int32_t r2;
    int16_t dx, dy;
    uint16_t x, y;

    if ((x1 > x2) || (y1 > y2))
    {
        return;
    }

    if (r > ((uint16_t)(x2 - x1) / 2U))
    {
        r = (uint16_t)((x2 - x1) / 2U);
    }
    if (r > ((uint16_t)(y2 - y1) / 2U))
    {
        r = (uint16_t)((y2 - y1) / 2U);
    }

    if (r == 0U)
    {
        LCD_Fill(x1, y1, x2, y2, color);
        return;
    }

    /* Central rectangle (strip between left and right corners) */
    LCD_Fill((uint16_t)(x1 + r), y1, (uint16_t)(x2 - r), y2, color);

    /* Left flat strip between top-left and bottom-left corners */
    LCD_Fill(x1, (uint16_t)(y1 + r), (uint16_t)(x1 + r - 1U), (uint16_t)(y2 - r), color);

    /* Right flat strip between top-right and bottom-right corners */
    LCD_Fill((uint16_t)(x2 - r + 1U), (uint16_t)(y1 + r), x2, (uint16_t)(y2 - r), color);

    /* Four corner quarters: outside corner pixels remain as background. */
    r2 = (int32_t)r * (int32_t)r;

    for (y = 0U; y < r; y++)
    {
        for (x = 0U; x < r; x++)
        {
            dx = (int16_t)r - (int16_t)x;
            dy = (int16_t)r - (int16_t)y;
            if ((int32_t)dx * (int32_t)dx + (int32_t)dy * (int32_t)dy <= r2)
            {
                /* Top-left corner */
                LCD_DrawPoint((uint16_t)(x1 + x),
                              (uint16_t)(y1 + y), color);
                /* Top-right corner */
                LCD_DrawPoint((uint16_t)(x2 - x),
                              (uint16_t)(y1 + y), color);
                /* Bottom-left corner */
                LCD_DrawPoint((uint16_t)(x1 + x),
                              (uint16_t)(y2 - y), color);
                /* Bottom-right corner */
                LCD_DrawPoint((uint16_t)(x2 - x),
                              (uint16_t)(y2 - y), color);
            }
        }
    }
}

void LCD_DrawRoundedRect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t r, uint32_t color)
{
    int32_t r2;
    int32_t inner_r2;
    int32_t dist2;
    int16_t dx, dy;
    uint16_t x, y;

    if ((x1 > x2) || (y1 > y2))
    {
        return;
    }

    if (r > ((uint16_t)(x2 - x1) / 2U))
    {
        r = (uint16_t)((x2 - x1) / 2U);
    }
    if (r > ((uint16_t)(y2 - y1) / 2U))
    {
        r = (uint16_t)((y2 - y1) / 2U);
    }

    if (r == 0U)
    {
        LCD_DrawRectangle(x1, y1, x2, y2, color);
        return;
    }

    /* Straight edge segments (between the corners) */
    LCD_DrawLine((uint16_t)(x1 + r), y1, (uint16_t)(x2 - r), y1, color);
    LCD_DrawLine((uint16_t)(x1 + r), y2, (uint16_t)(x2 - r), y2, color);
    LCD_DrawLine(x1, (uint16_t)(y1 + r), x1, (uint16_t)(y2 - r), color);
    LCD_DrawLine(x2, (uint16_t)(y1 + r), x2, (uint16_t)(y2 - r), color);

    /* Corner arcs — draw only pixels on the perimeter ring */
    r2 = (int32_t)r * (int32_t)r;
    inner_r2 = (int32_t)(r > 1U ? (r - 1U) : 0U) * (int32_t)(r > 1U ? (r - 1U) : 0U);

    for (y = 0U; y < r; y++)
    {
        for (x = 0U; x < r; x++)
        {
            dx = (int16_t)r - (int16_t)x;
            dy = (int16_t)r - (int16_t)y;
            dist2 = (int32_t)dx * (int32_t)dx + (int32_t)dy * (int32_t)dy;
            if (dist2 <= r2 && dist2 > inner_r2)
            {
                /* Top-left */
                LCD_DrawPoint((uint16_t)(x1 + x),
                              (uint16_t)(y1 + y), color);
                /* Top-right */
                LCD_DrawPoint((uint16_t)(x2 - x),
                              (uint16_t)(y1 + y), color);
                /* Bottom-left */
                LCD_DrawPoint((uint16_t)(x1 + x),
                              (uint16_t)(y2 - y), color);
                /* Bottom-right */
                LCD_DrawPoint((uint16_t)(x2 - x),
                              (uint16_t)(y2 - y), color);
            }
        }
    }
}

void LCD_ShowChar(uint16_t x, uint16_t y, char chr, uint8_t size, uint32_t fc, uint32_t bc)
{
    uint8_t temp;
    uint8_t row_block;
    uint8_t bit;
    uint16_t y0 = y;
    uint8_t bytes_per_char = (uint8_t)((size / 8 + ((size % 8) ? 1 : 0)) * (size / 2));
    const uint8_t *font = NULL;
    uint8_t chr_idx = (uint8_t)(chr - ' ');

    if ((chr < ' ') || (chr > '~'))
    {
        return;
    }

    switch (size)
    {
        case 12: font = asc2_1206[chr_idx]; break;
        case 16: font = asc2_1608[chr_idx]; break;
        case 24: font = asc2_2412[chr_idx]; break;
        case 32: font = asc2_3216[chr_idx]; break;
        default: return;
    }

    for (row_block = 0U; row_block < bytes_per_char; row_block++)
    {
        temp = font[row_block];
        for (bit = 0U; bit < 8U; bit++)
        {
            LCD_DrawPoint(x, y, (temp & 0x80U) ? fc : bc);
            temp <<= 1;
            y++;
            if ((y - y0) == size)
            {
                y = y0;
                x++;
                break;
            }
        }
    }
}

void LCD_ShowString(uint16_t x, uint16_t y, char *str, uint8_t size, uint32_t fc, uint32_t bc)
{
    if (str == NULL)
    {
        return;
    }

    while ((*str >= ' ') && (*str <= '~'))
    {
        LCD_ShowChar(x, y, *str, size, fc, bc);
        x = (uint16_t)(x + size / 2U);
        str++;
    }
}

void LCD_ShowChinese(uint16_t x, uint16_t y, const uint8_t *str, uint8_t size, uint32_t fc, uint32_t bc)
{
    const uint8_t *glyph;

    if (str == NULL)
    {
        return;
    }

    while (!((str[0] == 0U) && (str[1] == 0U)))
    {
        glyph = LCD_FindChineseGlyph(str, size);
        if (glyph != NULL)
        {
            LCD_DrawSquareGlyphColumnLSB(x, y, glyph, size, fc, bc);
        }
        else
        {
            LCD_Fill(x, y, (uint16_t)(x + size - 1U), (uint16_t)(y + size - 1U), bc);
        }

        x = (uint16_t)(x + size);
        str += 2;
    }
}

void LCD_DisplayStringLine(uint16_t y, uint8_t *str)
{
    LCD_ShowString(10U, y, (char *)str, 16U, g_text_color, g_back_color);
}

void LCD_SetBackColor(uint32_t color)
{
    g_back_color = color;
}

void LCD_SetTextColor(uint32_t color)
{
    g_text_color = color;
}

void LCD_Refresh(void)
{
    uint16_t *buffer = LCD_GetFrameBuffer();
    uint32_t total_pixels = LCD_PHYS_WIDTH * LCD_PHYS_HEIGHT;

    LCD_CleanDCacheRegion(buffer, total_pixels * sizeof(uint16_t));
}

void LCD_RefreshRect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    uint16_t phys_x0;
    uint16_t phys_x1;
    uint16_t phys_y0;
    uint16_t phys_y1;
    uint16_t phys_y;
    uint16_t *buffer = LCD_GetFrameBuffer();
    uint32_t row_bytes;

    if ((x1 >= LCD_WIDTH) || (y1 >= LCD_HEIGHT))
    {
        return;
    }

    if (x2 >= LCD_WIDTH)
    {
        x2 = (uint16_t)(LCD_WIDTH - 1U);
    }
    if (y2 >= LCD_HEIGHT)
    {
        y2 = (uint16_t)(LCD_HEIGHT - 1U);
    }
    if ((x1 > x2) || (y1 > y2))
    {
        return;
    }

    phys_x0 = y1;
    phys_x1 = y2;
    phys_y0 = (uint16_t)(LCD_WIDTH - 1U - x2);
    phys_y1 = (uint16_t)(LCD_WIDTH - 1U - x1);
    row_bytes = (uint32_t)(phys_x1 - phys_x0 + 1U) * sizeof(uint16_t);

    for (phys_y = phys_y0; phys_y <= phys_y1; phys_y++)
    {
        LCD_CleanDCacheRegion(&buffer[(uint32_t)phys_y * LCD_PHYS_WIDTH + phys_x0], row_bytes);
    }
}
