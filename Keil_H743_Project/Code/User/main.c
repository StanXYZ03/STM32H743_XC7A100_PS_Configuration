/**
  ******************************************************************************
  * @file    main.c
  * @brief   Course experiment LCD panel on top of the minimum LCD bring-up chain
  ******************************************************************************
  */

#include "main.h"

const char *get_mode_name(uint8_t mode);
const char *get_mode_short_name(uint8_t mode);
const char *get_frequency_name(uint8_t clk_sel);

/* -- M3 Material Design 3 Surface elevation palette (dark theme) -- */
#define UI_SURFACE                          0x070B10U  /* elevation 0 — page background */
#define UI_SURFACE_CONTAINER_LOW            0x16212BU  /* elevation 1 — main card */
#define UI_SURFACE_CONTAINER_LOWEST         0x213140U  /* elevation 1 darker — panel inside card */
#define UI_SURFACE_CONTAINER_HIGH           0x101820U  /* elevation 3 — header bar */
#define UI_M3_PRIMARY                       0x00BCD4U  /* M3 Primary — accent / status highlight */
#define UI_M3_TERTIARY                      0xFFC107U  /* M3 Tertiary — warn accent */

/* -- Backward-compatible aliases -- */
#define UI_BG_COLOR             UI_SURFACE
#define UI_HEADER_BG            UI_SURFACE_CONTAINER_HIGH
#define UI_CARD_BG              UI_SURFACE_CONTAINER_LOW
#define UI_CARD_ALT_BG          UI_SURFACE_CONTAINER_LOWEST
#define UI_ACCENT_COLOR         UI_M3_PRIMARY
#define UI_WARN_ACCENT_COLOR    UI_M3_TERTIARY

/* -- Unchanged detail colors -- */
#define UI_HEADER_SUB                       0xB8C7D1U
#define UI_CARD_CLOCK_BG                    0x121B24U
#define UI_BORDER_COLOR                     0x2E4050U
#define UI_LABEL_COLOR                      0xA8B7C3U
#define UI_VALUE_COLOR                      0xF5F8FAU
#define UI_MUTED_VALUE_COLOR                0xD5DEE6U
#define UI_STATUS_OK_BG                     0x2E7D32U
#define UI_STATUS_ERR_BG                    0xC62828U
#define UI_STATUS_WARN_BG                   0xF9A825U
#define UI_DIGIT_BG                         0x1A0B0BU
#define UI_DIGIT_BORDER                     0x7E2525U
#define UI_SEG_ON_COLOR                     0xFF2B24U
#define UI_SEG_OFF_COLOR                    0x3A1212U
#define UI_LED_ON_COLOR                     0x2E7D32U
#define UI_LED_OFF_COLOR                    0xCFD8DCU
#define UI_LED_BORDER                       0x90A4AEU
#define UI_STATUS_TEXT_Y                    18U
#define UI_CARD_TITLE_H                     24U
#define UI_HEADER_BOTTOM_Y                  56U
#define UI_REFRESH_PERIOD_MS                30U

#define STATUS_X1                           0U
#define STATUS_Y1                           0U
#define STATUS_X2                           799U
#define STATUS_Y2                           55U

#define MODE_X1                             16U
#define MODE_Y1                             72U
#define MODE_X2                             784U
#define MODE_Y2                             350U

#define FREQ_X1                             16U
#define FREQ_Y1                             366U
#define FREQ_X2                             784U
#define FREQ_Y2                             464U

#define DATA_X1                             16U
#define DATA_Y1                             72U
#define DATA_X2                             784U
#define DATA_Y2                             350U

#define CLOCK_X1                            16U
#define CLOCK_Y1                            366U
#define CLOCK_X2                            784U
#define CLOCK_Y2                            464U

#define AUX_LABEL_Y                         396U
#define AUX_VALUE_Y                         418U
#define AUX_FREQ_X                          38U
#define AUX_FREQ_VALUE_X1                   38U
#define AUX_FREQ_VALUE_Y1                   418U
#define AUX_FREQ_VALUE_X2                   244U
#define AUX_FREQ_VALUE_Y2                   450U
#define AUX_MODE_X                          254U
#define AUX_MODE_VALUE_X1                   254U
#define AUX_MODE_VALUE_Y1                   420U
#define AUX_MODE_VALUE_X2                   534U
#define AUX_MODE_VALUE_Y2                   448U
#define AUX_KEY_LED_X                       560U
#define AUX_KEY_LED_VALUE_X1                560U
#define AUX_KEY_LED_VALUE_Y1                396U
#define AUX_KEY_LED_VALUE_X2                760U
#define AUX_KEY_LED_VALUE_Y2                450U
#define HEADER_TITLE_X                      20U
#define HEADER_TITLE_Y                      4U
#define HEADER_TITLE_TEXT_SIZE            32U
#define HEADER_RESOURCE_X                   320U
#define HEADER_RESOURCE_Y                   38U
#define STATUS_LINK_LABEL_X                 684U
#define STATUS_LINK_VALUE_X                 724U

#define LCD_DIAGNOSTIC_MODE                 0U

static uint8_t spi_tx_buf[SPI1_DMA_BUFFER_SIZE] SPI1_DMA_ALIGNED;   /* Dummy MOSI pattern; XO2 slave only needs valid clocks/CS. */
static uint8_t spi_rx_buf[SPI1_DMA_BUFFER_SIZE] SPI1_DMA_ALIGNED;

static uint8_t current_mode = 0U;
static uint8_t current_clk_sel = 0U;
static uint32_t frame_count = 0U;
static uint16_t rx_word0 = 0U;
static uint16_t rx_word1 = 0U;
static uint16_t rx_word2 = 0U;
static uint16_t current_fmcu_pi = 0x0000U;
static uint32_t current_xc7a_po = 0x00000000UL;
static uint16_t current_xc7a_pio = 0x0000U;
static uint8_t current_key_led_mask = 0U;
static uint8_t payload_bytes[4] = {0U};
static uint8_t payload_nibbles[8] = {0U};
static HAL_StatusTypeDef last_spi_status = HAL_OK;
static const char *g_boot_stage = "reset";
/* -- GB2312-encoded Chinese UI label strings (null-terminated pairs) -- */
static const uint8_t CH_EXPERIMENT_VIEW[]   = {0xCA, 0xB5, 0xD1, 0xE9, 0xCA, 0xD3, 0xCD, 0xBC, 0x00, 0x00};
static const uint8_t CH_RUN_STATUS[]        = {0xD4, 0xCB, 0xD0, 0xD0, 0xD7, 0xB4, 0xCC, 0xAC, 0x00, 0x00};
static const uint8_t CH_CUR_MODE[]          = {0xB5, 0xB1, 0xC7, 0xB0, 0xC4, 0xA3, 0xCA, 0xBD, 0x00, 0x00};
static const uint8_t CH_LINK[]              = {0xC1, 0xB4, 0xC2, 0xB7, 0x00, 0x00};
static const uint8_t CH_READY[]             = {0xBE, 0xCD, 0xD0, 0xF7, 0x00, 0x00};
static const uint8_t CH_HOLD[]              = {0xB5, 0xC8, 0xB4, 0xFD, 0x00, 0x00};
static const uint8_t CH_PANEL_TITLE[]       = {0xBF, 0xCE, 0xB3, 0xCC, 0xCA, 0xB5, 0xD1, 0xE9, 0xC3, 0xE6, 0xB0, 0xE5, 0x00, 0x00};
static const uint8_t CH_FREQ[]              = {0xC6, 0xB5, 0xC2, 0xCA, 0x00, 0x00};
static const uint8_t CH_DIGITS[]            = {0xCA, 0xFD, 0xC2, 0xEB, 0xB9, 0xDC, 0x00, 0x00};
static const uint8_t CH_LED_GROUP[]         = {0x4C, 0x45, 0x44, 0xD7, 0xE9, 0x00, 0x00};
static const uint8_t CH_LINK_DISCONNECT[]   = {0xB6, 0xCF, 0xBF, 0xAA, 0x00, 0x00};

/* -- Mode-view label arrays (GB2312 encoded) -- */
static const uint8_t CH_RESULT[]           = {0xBD, 0xE1, 0xB9, 0xFB, 0x00, 0x00};
static const uint8_t CH_KEY_CHAIN[]        = {0xB0, 0xB4, 0xBC, 0xFC, 0xC1, 0xB4, 0x00, 0x00};
static const uint8_t CH_INPUT_BITS[]       = {0xCA, 0xE4, 0xC8, 0xEB, 0xCE, 0xBB, 0x00, 0x00};
static const uint8_t CH_DECODED_VALUE[]    = {0xD2, 0xEB, 0xC2, 0xEB, 0xD6, 0xB5, 0x00, 0x00};
static const uint8_t CH_SEG_PATTERN[]      = {0xB6, 0xCE, 0xC4, 0xA3, 0xCA, 0xBD, 0x00, 0x00};
static const uint8_t CH_LAST_EVENT[]       = {0xD7, 0xEE, 0xBD, 0xFC, 0xCA, 0xC2, 0xBC, 0xFE, 0x00, 0x00};
static const uint8_t CH_REGISTER[]         = {0xBC, 0xC4, 0xB4, 0xE6, 0xC6, 0xF7, 0x00, 0x00};
static const uint8_t CH_DIRECTION[]        = {0xB7, 0xBD, 0xCF, 0xF2, 0x00, 0x00};
static const uint8_t CH_SHIFT_PATH[]       = {0xD2, 0xC6, 0xCE, 0xBB, 0xC2, 0xB7, 0xBE, 0xB6, 0x00, 0x00};
static const uint8_t CH_COUNT[]            = {0xBC, 0xC6, 0xCA, 0xFD, 0xD6, 0xB5, 0x00, 0x00};
static const uint8_t CH_SCAN_LANES[]       = {0xC9, 0xA8, 0xC3, 0xE8, 0xCD, 0xA8, 0xB5, 0xC0, 0x00, 0x00};
static const uint8_t CH_VALUE[]            = {0xCA, 0xFD, 0xD6, 0xB5, 0x00, 0x00};
static const uint8_t CH_SINGLE_PULSE[]     = {0xB5, 0xA5, 0xC2, 0xF6, 0xB3, 0xE5, 0x00, 0x00};
static const uint8_t CH_LCD_OUTPUT[]       = {0x4C, 0x43, 0x44, 0x20, 0xCA, 0xE4, 0xB3, 0xF6, 0x00, 0x00};
static const uint8_t CH_GROUP[]            = {0xB7, 0xD6, 0xD7, 0xE9, 0x00, 0x00};
static const uint8_t CH_SERIAL[]           = {0xB4, 0xAE, 0xD0, 0xD0, 0x00, 0x00};
static const uint8_t CH_GROUP_LINES[]      = {0xD7, 0xE9, 0xCF, 0xDF, 0x00, 0x00};
static const uint8_t CH_CODE[]             = {0xBC, 0xFC, 0xC2, 0xEB, 0x00, 0x00};
static const uint8_t CH_PARAM[]            = {0xB2, 0xCE, 0xCA, 0xFD, 0x00, 0x00};
static const uint8_t CH_DISPLAY[]          = {0xCF, 0xD4, 0xCA, 0xBE, 0x00, 0x00};
static const uint8_t CH_SCAN[]             = {0xC9, 0xA8, 0xC3, 0xE8, 0x00, 0x00};
static const uint8_t CH_DIRECTION_KEYS[]   = {0xB7, 0xBD, 0xCF, 0xF2, 0xBC, 0xFC, 0x00, 0x00};
static const uint8_t CH_TIME[]             = {0xCA, 0xB1, 0xBC, 0xE4, 0x00, 0x00};
static const uint8_t CH_CONTROL[]          = {0xBF, 0xD8, 0xD6, 0xC6, 0x00, 0x00};
static const uint8_t CH_CONTROL_KEYS[]     = {0xBF, 0xD8, 0xD6, 0xC6, 0xBC, 0xFC, 0x00, 0x00};
static const uint8_t CH_OUTPUT[]           = {0xCA, 0xE4, 0xB3, 0xF6, 0x00, 0x00};
static const uint8_t CH_STATUS[]           = {0xD7, 0xB4, 0xCC, 0xAC, 0x00, 0x00};
static const uint8_t CH_LEFT[]             = {0xD7, 0xF3, 0xD2, 0xC6, 0x00, 0x00};
static const uint8_t CH_RIGHT[]            = {0xD3, 0xD2, 0xD2, 0xC6, 0x00, 0x00};
static const uint8_t CH_ACTIVE[]           = {0xBB, 0xEE, 0xB6, 0xAF, 0x00, 0x00};
static const uint8_t CH_IDLE[]             = {0xBF, 0xD5, 0xCF, 0xD0, 0x00, 0x00};
static const uint8_t CH_RUN[]              = {0xD4, 0xCB, 0xD0, 0xD0, 0x00, 0x00};
static const uint8_t CH_SET[]              = {0xC9, 0xE8, 0xD6, 0xC3, 0x00, 0x00};
static const uint8_t CH_MARK[]             = {0xB1, 0xEA, 0xBC, 0xC7, 0x00, 0x00};
static const uint8_t CH_SPACE[]            = {0xBF, 0xD5, 0xBA, 0xC5, 0x00, 0x00};
static const uint8_t CH_ALT[]              = {0xB1, 0xB8, 0xD3, 0xC3, 0x00, 0x00};
static const uint8_t CH_BASE[]             = {0xBB, 0xF9, 0xB4, 0xA1, 0x00, 0x00};
static const uint8_t CH_UP[]               = {0xC9, 0xCF, 0x00, 0x00};
static const uint8_t CH_DOWN[]             = {0xCF, 0xC2, 0x00, 0x00};
static const uint8_t CH_CENTER[]           = {0xD6, 0xD0, 0x00, 0x00};
static const uint8_t CH_ARROW_LEFT[]       = {0xD7, 0xF3, 0xCF, 0xF2, 0x00, 0x00};
static const uint8_t CH_ARROW_RIGHT[]      = {0xD3, 0xD2, 0xCF, 0xF2, 0x00, 0x00};
static const uint8_t CH_PIO_STATUS[]       = {0x50, 0x49, 0x4F, 0xD7, 0xB4, 0xCC, 0xAC, 0x00, 0x00};

static const char MODE0_RESOURCE_SUBTITLE[] = "8SEG + 12LED + LCD";
static const char MODE1_RESOURCE_SUBTITLE[] = "16BIT PI -> 4/8SEG";
static const char MODE2_RESOURCE_SUBTITLE[] = "AUTO/STUDENT DECODER";
static const char MODE3_RESOURCE_SUBTITLE[] = "8KEY EVENT + LATCH";
static const char MODE4_RESOURCE_SUBTITLE[] = "16SW BUS + LED";
static const char MODE5_RESOURCE_SUBTITLE[] = "RAW DIGIT + SEG CTRL";
static const char MODE6_RESOURCE_SUBTITLE[] = "8KEY SHAPE + LCD";
static const char MODE7_RESOURCE_SUBTITLE[] = "4x4 KEY MATRIX";
static const char MODE8_RESOURCE_SUBTITLE[] = "EC11 + PARAM VIEW";
static const char MODE9_RESOURCE_SUBTITLE[] = "5WAY NAV + LCD";
static const char MODEA_RESOURCE_SUBTITLE[] = "KEY/SW/SEG/LCD";
static const char MODEB_RESOURCE_SUBTITLE[] = "ADC/DAC/DDS VIEW";
static const char MODEC_RESOURCE_SUBTITLE[] = "ETH/USB REMOTE";

typedef void (*ModeViewRenderer)(void);

typedef struct
{
    uint8_t mode;
    const char *resource_subtitle;
    uint8_t compact_aux_mode_text;
    ModeViewRenderer draw_view;
    uint8_t has_lcd_output;
    uint8_t shows_key_led_strip;
    uint8_t uses_key_chain_display;
} ModeUiConfig;

typedef struct
{
    uint8_t previous_mode;
    uint8_t previous_visible_mask;
    uint8_t latched_mask;
    uint8_t pulse_ticks[KEY_LED_COUNT];
} KeyLedState;

typedef struct
{
    uint8_t visible_mask;
    uint8_t direct_mask;
    uint8_t latched_mask;
    uint8_t pulse_source;
    uint8_t pulse_mask;
} KeyLedSources;

typedef struct
{
    uint32_t digit_word;
    uint16_t led_status;
} LcdMode0Status;

typedef struct
{
    uint8_t key_value;
    uint8_t key_valid;
    uint8_t key_event;
    uint8_t multi_key;
    uint8_t key_pressed;
    uint8_t row_idx;
    uint8_t col_idx;
    uint16_t business_pio;
    uint32_t business_po;
} LcdMode7Status;

typedef struct
{
    uint16_t div_value;
    uint8_t tick_count;
    uint8_t step_select;
    uint8_t clk_div2;
    uint8_t cw_pulse;
    uint8_t ccw_pulse;
    uint8_t display_nibbles[8];
    uint16_t led_status;
} LcdMode8Status;

typedef struct
{
    uint8_t cursor_onehot;
    uint8_t cursor_index;
    uint8_t event_code;
    uint8_t boundary_hit;
    uint8_t key_mask;
} LcdMode9Status;

typedef struct
{
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t clock_state;
    uint8_t edit_select;
    uint8_t tick_1hz;
    uint16_t control_status;
} LcdModeAStatus;

static void Data_Process(void);
static uint16_t BuildFmcuPiValue(void);
static void UpdateKeyLedState(void);
static void KeyLedState_Reset(KeyLedState *state);
static void KeyLedState_SelectSources(KeyLedState *state, KeyLedSources *sources);
static void KeyLedState_ApplyPulse(KeyLedState *state, KeyLedSources *sources);
static void KeyLedState_Commit(const KeyLedSources *sources);
static uint8_t ApplyMode6KeyLedDisplayMap(uint8_t pio_mask);
static const ModeUiConfig *get_mode_ui_config(uint8_t mode);
static uint8_t mode_has_lcd_output(uint8_t mode);
static uint8_t mode_shows_key_led_strip(uint8_t mode);
static uint8_t mode_uses_key_chain_display(uint8_t mode);
static const char *get_mode_resource_subtitle(uint8_t mode);
static void LCD_Display_Update(void);
static void LCD_DrawStaticLayout(void);
static void LCD_DrawCard(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                         const uint8_t *title, uint32_t fill_color, uint32_t border_color);
static void LCD_ShowFormatted(uint16_t x, uint16_t y, uint8_t size,
                              uint32_t color, uint32_t back_color, const char *fmt, ...);
static void LCD_ClearCardContent(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint32_t fill_color);
static void LCD_ClearDynamicRegions(void);
static void LCD_DrawValuePanel(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                               const char *label, const char *value, uint32_t value_color);
static void LCD_DrawValuePanelCN(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                                 const uint8_t *label, const char *value, uint32_t value_color);
static void LCD_DrawNibbleRow(uint16_t x, uint16_t y, uint8_t first, uint8_t count);
static void LCD_DrawBitStrip(uint16_t x, uint16_t y, uint8_t count, uint16_t value, const uint8_t *label);
static void LCD_DrawPianoKeyStrip(uint16_t x, uint16_t y, uint8_t key_mask);
static void LCD_DrawKeyLedStrip(uint8_t led_mask);
static void LCD_DrawMatrix4x4(uint16_t x, uint16_t y, uint16_t active_mask);
static void LCD_DrawSegmentH(uint16_t x, uint16_t y, uint16_t w, uint16_t t, uint32_t color);
static void LCD_DrawSegmentV(uint16_t x, uint16_t y, uint16_t h, uint16_t t, uint32_t color);
static void LCD_DrawSevenSegDigit(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t value);
static LcdMode0Status LCD_ParseMode0Status(uint32_t po_status, uint16_t pio_status);
static uint8_t LCD_MapMode7LogicalKey(uint8_t row_idx, uint8_t col_idx);
static LcdMode7Status LCD_ParseMode7Status(uint32_t po_status, uint16_t pio_status);
static LcdMode8Status LCD_ParseMode8Status(uint32_t po_status, uint16_t pio_status);
static LcdMode9Status LCD_ParseMode9Status(uint32_t po_status, uint16_t pio_status);
static LcdModeAStatus LCD_ParseModeAStatus(uint32_t po_status, uint16_t pio_status);
static void LCD_DrawMode0View(void);
static void LCD_DrawMode0SevenSegView(uint32_t po_value);
static void LCD_DrawMode0LedView(uint16_t pio_value);
static void LCD_DrawMode1View(void);
static void LCD_DrawMode2View(void);
static void LCD_DrawMode3View(void);
static void LCD_DrawMode4View(void);
static void LCD_DrawMode5View(void);
static void LCD_DrawMode6View(void);
static void LCD_DrawMode7View(void);
static void LCD_DrawMode8View(void);
static uint8_t LCD_GetMode9CursorPos(uint8_t cursor_onehot, uint8_t fallback_index);
static void LCD_DrawMode9View(void);
static void LCD_DrawModeAView(void);
static void LCD_DrawModeBView(void);
static void LCD_DrawModeCView(void);
static void LCD_UpdateStatusCard(uint32_t status_bg, uint32_t status_text);
static void LCD_UpdateDataCard(void);
static void LCD_UpdateAuxCard(void);
static void SDRAM_BasicTest(void);
static void Framebuffer_Init(void);
static void LCD_FillFramebufferColor(uint16_t rgb565);
static void LCD_DiagnosticTest(void);

static const ModeUiConfig mode_ui_config_table[] = {
    {0x0U, MODE0_RESOURCE_SUBTITLE, 0U, LCD_DrawMode0View, 1U, 1U, 0U},
    {0x1U, MODE1_RESOURCE_SUBTITLE, 0U, LCD_DrawMode1View, 0U, 1U, 1U},
    {0x2U, MODE2_RESOURCE_SUBTITLE, 0U, LCD_DrawMode2View, 0U, 1U, 1U},
    {0x3U, MODE3_RESOURCE_SUBTITLE, 0U, LCD_DrawMode3View, 0U, 1U, 1U},
    {0x4U, MODE4_RESOURCE_SUBTITLE, 0U, LCD_DrawMode4View, 0U, 0U, 0U},
    {0x5U, MODE5_RESOURCE_SUBTITLE, 0U, LCD_DrawMode5View, 1U, 0U, 0U},
    {0x6U, MODE6_RESOURCE_SUBTITLE, 0U, LCD_DrawMode6View, 0U, 1U, 0U},
    {0x7U, MODE7_RESOURCE_SUBTITLE, 1U, LCD_DrawMode7View, 1U, 0U, 0U},
    {0x8U, MODE8_RESOURCE_SUBTITLE, 1U, LCD_DrawMode8View, 1U, 0U, 0U},
    {0x9U, MODE9_RESOURCE_SUBTITLE, 1U, LCD_DrawMode9View, 1U, 0U, 0U},
    {0xAU, MODEA_RESOURCE_SUBTITLE, 1U, LCD_DrawModeAView, 1U, 1U, 0U},
    {0xBU, MODEB_RESOURCE_SUBTITLE, 0U, LCD_DrawModeBView, 0U, 0U, 0U},
    {0xCU, MODEC_RESOURCE_SUBTITLE, 0U, LCD_DrawModeCView, 0U, 0U, 0U},
};

#define BOOT_STAGE(_stage) do { g_boot_stage = (_stage); } while (0)

int main(void)
{
    BOOT_STAGE("MPU_Config");
    MPU_Config();
    BOOT_STAGE("CPU_CACHE_Enable");
    CPU_CACHE_Enable();
    HAL_Init();
    BOOT_STAGE("SystemClock_Config");
    SystemClock_Config();

    BOOT_STAGE("MX_GPIO_Init");
    MX_GPIO_Init();
    BOOT_STAGE("SPI1_Master_Init");
    SPI1_Master_Init();

    BOOT_STAGE("bsp_InitExtSDRAM");
    bsp_InitExtSDRAM();
    BOOT_STAGE("SDRAM_BasicTest");
    SDRAM_BasicTest();
    BOOT_STAGE("Framebuffer_Init");
    Framebuffer_Init();

    BOOT_STAGE("LCD_RGB_Init");
    LCD_RGB_Init();

    BOOT_STAGE("LTDC_CFBAR_Check");
    if (LTDC_Layer1->CFBAR != LCD_RGB_FB_ADDR)
    {
        LTDC_Layer1->CFBAR = LCD_RGB_FB_ADDR;
        LTDC->SRCR = LTDC_SRCR_IMR;
    }

    BOOT_STAGE("LCD_RGB_BacklightOn");
    LCD_RGB_BacklightOn();

#if (LCD_DIAGNOSTIC_MODE == 1U)
    BOOT_STAGE("LCD_DiagnosticTest");
    LCD_DiagnosticTest();
#endif

    LCD_SetBackColor(LCD_COLOR_BLACK);
    LCD_SetTextColor(LCD_COLOR_WHITE);
    LCD_DrawStaticLayout();

    /* Dummy TX pattern: FPGA slave only uses MOSI for CS-edge timing. */
    spi_tx_buf[0] = 0xA5U;
    spi_tx_buf[1] = 0x5AU;
    spi_tx_buf[2] = 0x00U;
    spi_tx_buf[3] = 0x00U;
    spi_tx_buf[4] = 0x00U;
    spi_tx_buf[5] = 0x00U;

    LCD_Display_Update();

    while (1)
    {
        last_spi_status = SPI1_SendReceive(spi_tx_buf, spi_rx_buf, SPI_FRAME_SIZE);
        if (last_spi_status == HAL_OK)
        {
            Data_Process();
        }

        UpdateKeyLedState();
        LCD_Display_Update();

        HAL_Delay(UI_REFRESH_PERIOD_MS);
        frame_count++;
    }
}

static void Data_Process(void)
{
    /* FPGA spi_packet_builder.v packs 96 bits MSB-first as
     *   {clk_sel[7:0], 4'b0000, mode[3:0], payload[31:0], PO[31:0], PIO[15:0]}
     * After MSB-first 12-byte transfer:
     *   rx[0] = clk_sel           rx[1] = {0000, mode}
     *   rx[2..5] = 4 legacy-aligned control bytes
     *   rx[6..9] = XC7A PO[31:0]  rx[10..11] = XC7A PIO[15:0]
     */
    current_clk_sel = spi_rx_buf[0] & 0x1FU;
    current_mode    = spi_rx_buf[1] & 0x0FU;

    payload_bytes[0] = spi_rx_buf[2];
    payload_bytes[1] = spi_rx_buf[3];
    payload_bytes[2] = spi_rx_buf[4];
    payload_bytes[3] = spi_rx_buf[5];

    payload_nibbles[0] = (uint8_t)(spi_rx_buf[2] >> 4);
    payload_nibbles[1] = (uint8_t)(spi_rx_buf[2] & 0x0FU);
    payload_nibbles[2] = (uint8_t)(spi_rx_buf[3] >> 4);
    payload_nibbles[3] = (uint8_t)(spi_rx_buf[3] & 0x0FU);
    payload_nibbles[4] = (uint8_t)(spi_rx_buf[4] >> 4);
    payload_nibbles[5] = (uint8_t)(spi_rx_buf[4] & 0x0FU);
    payload_nibbles[6] = (uint8_t)(spi_rx_buf[5] >> 4);
    payload_nibbles[7] = (uint8_t)(spi_rx_buf[5] & 0x0FU);

    rx_word0 = ((uint16_t)spi_rx_buf[0] << 8) | spi_rx_buf[1];
    rx_word1 = ((uint16_t)spi_rx_buf[2] << 8) | spi_rx_buf[3];
    rx_word2 = ((uint16_t)spi_rx_buf[4] << 8) | spi_rx_buf[5];
    current_xc7a_po = ((uint32_t)spi_rx_buf[6] << 24) |
                      ((uint32_t)spi_rx_buf[7] << 16) |
                      ((uint32_t)spi_rx_buf[8] << 8)  |
                      ((uint32_t)spi_rx_buf[9]);
    current_xc7a_pio = ((uint16_t)spi_rx_buf[10] << 8) |
                       ((uint16_t)spi_rx_buf[11]);

    current_fmcu_pi = BuildFmcuPiValue();
    FMCU_PI_Write(current_fmcu_pi);
}

static uint16_t BuildFmcuPiValue(void)
{
    const uint8_t control_byte_one   = payload_bytes[0];
    const uint8_t control_byte_two   = payload_bytes[1];
    const uint8_t control_byte_three = payload_bytes[2];
    const uint8_t control_byte_four  = payload_bytes[3];
    uint16_t value = 0x0000U;

    switch (current_mode)
    {
        case 0x0U:
            value |= (uint16_t)control_byte_one;
            value |= (uint16_t)((control_byte_two & 0x01U) << 8);
            value |= (uint16_t)(((control_byte_two >> 4) & 0x01U) << 9);
            value |= (uint16_t)((control_byte_three & 0x01U) << 10);
            value |= (uint16_t)(((control_byte_three >> 4) & 0x01U) << 11);
            value |= (uint16_t)((control_byte_four & 0x01U) << 12);
            value |= (uint16_t)(((control_byte_four >> 4) & 0x01U) << 13);
            break;

        case 0x6U:
            value = (uint16_t)control_byte_one;
            break;

        case 0x1U:
            value |= (uint16_t)control_byte_two;
            value |= (uint16_t)((uint16_t)control_byte_one << 8);
            break;

        case 0x2U:
        case 0x3U:
            value |= (uint16_t)((control_byte_one & 0x01U) << 0);
            value |= (uint16_t)(((control_byte_one >> 4) & 0x01U) << 1);
            value |= (uint16_t)((control_byte_two & 0x01U) << 2);
            value |= (uint16_t)(((control_byte_two >> 4) & 0x01U) << 3);
            value |= (uint16_t)((control_byte_three & 0x01U) << 4);
            value |= (uint16_t)(((control_byte_three >> 4) & 0x01U) << 5);
            value |= (uint16_t)((control_byte_four & 0x01U) << 6);
            value |= (uint16_t)(((control_byte_four >> 4) & 0x01U) << 7);
            break;

        default:
            break;
    }

    return value;
}

static void UpdateKeyLedState(void)
{
    static KeyLedState state = {0xFFU, 0U, 0U, {0U}};
    KeyLedSources sources;

    if (last_spi_status != HAL_OK)
    {
        KeyLedState_Reset(&state);
        KeyLedState_Commit(NULL);
        return;
    }

    if (state.previous_mode != current_mode)
    {
        KeyLedState_Reset(&state);
        state.previous_mode = current_mode;
    }

    if (mode_shows_key_led_strip(current_mode) == 0U)
    {
        KeyLedState_Reset(&state);
        KeyLedState_Commit(NULL);
        return;
    }

    KeyLedState_SelectSources(&state, &sources);
    KeyLedState_ApplyPulse(&state, &sources);
    KeyLedState_Commit(&sources);
}

static void KeyLedState_Reset(KeyLedState *state)
{
    uint8_t i;

    state->previous_mode = 0xFFU;
    state->previous_visible_mask = 0U;
    state->latched_mask = 0U;
    for (i = 0U; i < KEY_LED_COUNT; i++)
    {
        state->pulse_ticks[i] = 0U;
    }
}

static void KeyLedState_SelectSources(KeyLedState *state, KeyLedSources *sources)
{
    uint8_t visible_mask = (uint8_t)(current_fmcu_pi & 0x00FFU);
    uint8_t direct_mask = 0U;
    uint8_t latched_mask = state->latched_mask;
    uint8_t pulse_source = 0U;

    switch (current_mode)
    {
        case 0x0U:
            visible_mask = (uint8_t)(
                0x03U
                | (((current_fmcu_pi >> 8)  & 0x0001U) << 2)
                | (((current_fmcu_pi >> 9)  & 0x0001U) << 3)
                | (((current_fmcu_pi >> 10) & 0x0001U) << 4)
                | (((current_fmcu_pi >> 11) & 0x0001U) << 5)
                | (((current_fmcu_pi >> 12) & 0x0001U) << 6)
                | (((current_fmcu_pi >> 13) & 0x0001U) << 7)
            );
            direct_mask = visible_mask;
            break;

        case 0x1U:
            latched_mask = 0x0FU;
            direct_mask = (uint8_t)((current_xc7a_pio & 0x000FU) << 4);
            break;

        case 0x2U:
            direct_mask = visible_mask;
            break;

        case 0x3U:
            pulse_source = visible_mask;
            break;

        case 0x6U:
            direct_mask = (uint8_t)(current_xc7a_pio & 0x00FFU);
            direct_mask = ApplyMode6KeyLedDisplayMap(direct_mask);
            break;

        case 0xAU:
            direct_mask = (uint8_t)(current_xc7a_pio & 0x00FFU);
            break;

        case 0xBU:
        case 0xCU:
            direct_mask = 0U;
            break;

        default:
            direct_mask = 0U;
            break;
    }

    state->latched_mask = latched_mask;
    sources->visible_mask = visible_mask;
    sources->direct_mask = direct_mask;
    sources->latched_mask = latched_mask;
    sources->pulse_source = pulse_source;
    sources->pulse_mask = 0U;
}

static void KeyLedState_ApplyPulse(KeyLedState *state, KeyLedSources *sources)
{
    uint8_t rising_mask = (uint8_t)(sources->pulse_source & (uint8_t)(~state->previous_visible_mask));
    uint8_t i;

    state->previous_visible_mask = sources->visible_mask;

    for (i = 0U; i < KEY_LED_COUNT; i++)
    {
        if ((rising_mask & (uint8_t)(1U << i)) != 0U)
        {
            state->pulse_ticks[i] = 2U;
        }

        if (state->pulse_ticks[i] != 0U)
        {
            sources->pulse_mask = (uint8_t)(sources->pulse_mask | (uint8_t)(1U << i));
            state->pulse_ticks[i]--;
        }
    }
}

static void KeyLedState_Commit(const KeyLedSources *sources)
{
    if (sources == NULL)
    {
        current_key_led_mask = 0U;
    }
    else
    {
        current_key_led_mask = (uint8_t)(sources->direct_mask |
                                         sources->latched_mask |
                                         sources->pulse_mask);
    }
    KEY_LED_WriteMask(current_key_led_mask);
}

static uint8_t ApplyMode6KeyLedDisplayMap(uint8_t pio_mask)
{
    uint8_t bit1 = (uint8_t)((pio_mask >> 1U) & 0x01U);
    uint8_t bit3 = (uint8_t)((pio_mask >> 3U) & 0x01U);

    pio_mask &= (uint8_t)~((1U << 1U) | (1U << 3U));
    pio_mask |= (uint8_t)((uint8_t)(bit1 << 3U) | (uint8_t)(bit3 << 1U));

    return pio_mask;
}

static const ModeUiConfig *get_mode_ui_config(uint8_t mode)
{
    uint8_t i;

    for (i = 0U; i < (sizeof(mode_ui_config_table) / sizeof(mode_ui_config_table[0])); i++)
    {
        if (mode_ui_config_table[i].mode == mode)
        {
            return &mode_ui_config_table[i];
        }
    }

    return &mode_ui_config_table[0xBU];
}

static uint8_t mode_has_lcd_output(uint8_t mode)
{
    return get_mode_ui_config(mode)->has_lcd_output;
}

static uint8_t mode_shows_key_led_strip(uint8_t mode)
{
    return get_mode_ui_config(mode)->shows_key_led_strip;
}

static uint8_t mode_uses_key_chain_display(uint8_t mode)
{
    return get_mode_ui_config(mode)->uses_key_chain_display;
}

static const char *get_mode_resource_subtitle(uint8_t mode)
{
    return get_mode_ui_config(mode)->resource_subtitle;
}

static void LCD_DrawStaticLayout(void)
{
    LCD_Clear(UI_BG_COLOR);
    LCD_Fill(0U, 0U, LCD_WIDTH - 1U, UI_HEADER_BOTTOM_Y, UI_HEADER_BG);
    LCD_DrawLine(0U, UI_HEADER_BOTTOM_Y, LCD_WIDTH - 1U, UI_HEADER_BOTTOM_Y, UI_BORDER_COLOR);
    LCD_ShowChinese(20U, 12U, CH_PANEL_TITLE, 32U, LCD_COLOR_WHITE, UI_HEADER_BG);
    LCD_DrawCard(DATA_X1, DATA_Y1, DATA_X2, DATA_Y2, CH_EXPERIMENT_VIEW,
                 UI_CARD_BG, UI_BORDER_COLOR);
    LCD_DrawCard(CLOCK_X1, CLOCK_Y1, CLOCK_X2, CLOCK_Y2, CH_RUN_STATUS,
                 UI_CARD_CLOCK_BG, UI_BORDER_COLOR);
    LCD_Refresh();
}

static void LCD_DrawCard(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                         const uint8_t *title, uint32_t fill_color, uint32_t border_color)
{
    LCD_FillRounded(x1, y1, x2, y2, 10U, fill_color);
    LCD_FillRounded((uint16_t)(x1 + 1U), (uint16_t)(y1 + 1U),
                    (uint16_t)(x2 - 1U), (uint16_t)(y1 + UI_CARD_TITLE_H),
                    9U, border_color);
    LCD_DrawRoundedRect(x1, y1, x2, y2, 10U, border_color);
    LCD_ShowChinese((uint16_t)(x1 + 12U), (uint16_t)(y1 + 5U), title, 16U,
                    LCD_COLOR_WHITE, border_color);
}

static void LCD_ShowFormatted(uint16_t x, uint16_t y, uint8_t size,
                              uint32_t color, uint32_t back_color, const char *fmt, ...)
{
    char line_buf[96];
    va_list args;

    va_start(args, fmt);
    vsnprintf(line_buf, sizeof(line_buf), fmt, args);
    va_end(args);

    LCD_ShowString(x, y, line_buf, size, color, back_color);
}

static void LCD_ClearCardContent(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint32_t fill_color)
{
    uint16_t content_top = (uint16_t)(y1 + UI_CARD_TITLE_H + 1U);
    uint16_t rounded_top = (uint16_t)(y2 - 18U);

    LCD_Fill((uint16_t)(x1 + 1U), content_top,
             (uint16_t)(x2 - 1U), rounded_top, fill_color);
    LCD_FillRounded((uint16_t)(x1 + 1U), rounded_top,
                    (uint16_t)(x2 - 1U), (uint16_t)(y2 - 1U),
                    9U, fill_color);
}

static void LCD_ClearDynamicRegions(void)
{
    LCD_ClearCardContent(DATA_X1, DATA_Y1, DATA_X2, DATA_Y2, UI_CARD_BG);
    LCD_ClearCardContent(CLOCK_X1, CLOCK_Y1, CLOCK_X2, CLOCK_Y2, UI_CARD_CLOCK_BG);
}

static void LCD_DrawValuePanel(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                               const char *label, const char *value, uint32_t value_color)
{
    uint8_t value_size = (strlen(value) > 6U) ? 24U : 32U;

    LCD_FillRounded(x1, y1, x2, y2, 8U, UI_CARD_ALT_BG);
    LCD_DrawRoundedRect(x1, y1, x2, y2, 8U, UI_BORDER_COLOR);
    LCD_ShowFormatted((uint16_t)(x1 + 16U), (uint16_t)(y1 + 10U), 16U,
                      UI_LABEL_COLOR, UI_CARD_ALT_BG, "%s", label);
    LCD_ShowFormatted((uint16_t)(x1 + 16U), (uint16_t)(y1 + 34U), value_size,
                      value_color, UI_CARD_ALT_BG, "%s", value);
}

static void LCD_DrawValuePanelCN(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                                 const uint8_t *label, const char *value, uint32_t value_color)
{
    uint8_t value_size = (strlen(value) > 6U) ? 24U : 32U;

    LCD_FillRounded(x1, y1, x2, y2, 8U, UI_CARD_ALT_BG);
    LCD_DrawRoundedRect(x1, y1, x2, y2, 8U, UI_BORDER_COLOR);
    LCD_ShowChinese((uint16_t)(x1 + 16U), (uint16_t)(y1 + 10U), label, 16U,
                    UI_LABEL_COLOR, UI_CARD_ALT_BG);
    LCD_ShowFormatted((uint16_t)(x1 + 16U), (uint16_t)(y1 + 34U), value_size,
                      value_color, UI_CARD_ALT_BG, "%s", value);
}

static void LCD_DrawNibbleRow(uint16_t x, uint16_t y, uint8_t first, uint8_t count)
{
    uint8_t i;
    uint16_t px;

    for (i = 0U; i < count; i++)
    {
        px = (uint16_t)(x + (uint16_t)i * 54U);
        LCD_FillRounded(px, y, (uint16_t)(px + 40U), (uint16_t)(y + 46U), 6U, UI_CARD_ALT_BG);
        LCD_DrawRoundedRect(px, y, (uint16_t)(px + 40U), (uint16_t)(y + 46U), 6U, UI_BORDER_COLOR);
        LCD_ShowFormatted((uint16_t)(px + 11U), (uint16_t)(y + 8U), 32U,
                          UI_WARN_ACCENT_COLOR, UI_CARD_ALT_BG, "%X",
                          payload_nibbles[(first + i) & 0x07U]);
    }
}

static void LCD_DrawBitStrip(uint16_t x, uint16_t y, uint8_t count, uint16_t value, const uint8_t *label)
{
    uint8_t bit;
    uint8_t on;
    uint16_t px;
    const uint16_t cell_w = 34U;
    const uint16_t gap = 10U;

    LCD_ShowChinese(x, (uint16_t)(y - 24U), label, 16U, UI_LABEL_COLOR, UI_CARD_BG);

    for (bit = 0U; bit < count; bit++)
    {
        on = ((value & (uint16_t)(1U << bit)) != 0U) ? 1U : 0U;
        px = (uint16_t)(x + (uint16_t)bit * (cell_w + gap));
        LCD_FillRounded(px, y, (uint16_t)(px + cell_w), (uint16_t)(y + 22U), 5U,
                        on ? UI_ACCENT_COLOR : UI_CARD_ALT_BG);
        LCD_DrawRoundedRect(px, y, (uint16_t)(px + cell_w), (uint16_t)(y + 22U), 5U, UI_BORDER_COLOR);
    }
}

static void LCD_DrawPianoKeyStrip(uint16_t x, uint16_t y, uint8_t key_mask)
{
    static const uint8_t piano_black_after[7] = {1U, 1U, 0U, 1U, 1U, 1U, 0U};
    uint8_t key;
    uint8_t on;
    uint16_t key_x;
    uint16_t black_x;
    const uint16_t white_w = 76U;
    const uint16_t white_h = 120U;
    const uint16_t black_w = 38U;
    const uint16_t black_h = 74U;

    for (key = 0U; key < 8U; key++)
    {
        on = ((key_mask & (uint8_t)(1U << key)) != 0U) ? 1U : 0U;
        key_x = (uint16_t)(x + (uint16_t)key * white_w);
        LCD_FillRounded(key_x, y, (uint16_t)(key_x + white_w - 2U),
                        (uint16_t)(y + white_h), 7U,
                        on ? UI_WARN_ACCENT_COLOR : LCD_COLOR_WHITE);
        LCD_DrawRoundedRect(key_x, y, (uint16_t)(key_x + white_w - 2U),
                            (uint16_t)(y + white_h), 7U, UI_BORDER_COLOR);
        LCD_ShowFormatted((uint16_t)(key_x + 26U), (uint16_t)(y + 82U), 24U,
                          on ? UI_BG_COLOR : UI_BORDER_COLOR,
                          on ? UI_WARN_ACCENT_COLOR : LCD_COLOR_WHITE,
                          "%u", (unsigned int)(key + 1U));
    }

    for (key = 0U; key < 7U; key++)
    {
        if (piano_black_after[key] != 0U)
        {
            black_x = (uint16_t)(x + (uint16_t)(key + 1U) * white_w - (black_w / 2U));
            LCD_FillRounded(black_x, y, (uint16_t)(black_x + black_w),
                            (uint16_t)(y + black_h), 5U, LCD_COLOR_BLACK);
            LCD_DrawRoundedRect(black_x, y, (uint16_t)(black_x + black_w),
                                (uint16_t)(y + black_h), 5U, UI_BORDER_COLOR);
        }
    }
}

static void LCD_DrawKeyLedStrip(uint8_t led_mask)
{
    uint8_t led;
    uint8_t on;
    uint16_t x;
    const uint16_t y = 424U;
    const uint16_t led_w = 17U;
    const uint16_t led_h = 20U;
    const uint16_t gap = 7U;

    LCD_ShowFormatted(AUX_KEY_LED_X, AUX_LABEL_Y, 16U, UI_LABEL_COLOR, UI_CARD_CLOCK_BG, "KEY LED");

    for (led = 0U; led < KEY_LED_COUNT; led++)
    {
        on = ((led_mask & (uint8_t)(1U << led)) != 0U) ? 1U : 0U;
        x = (uint16_t)(AUX_KEY_LED_X + (uint16_t)led * (led_w + gap));
        LCD_FillRounded(x, y, (uint16_t)(x + led_w), (uint16_t)(y + led_h), 4U,
                        on ? UI_LED_ON_COLOR : UI_CARD_ALT_BG);
        LCD_DrawRoundedRect(x, y, (uint16_t)(x + led_w), (uint16_t)(y + led_h), 4U,
                            on ? UI_LED_BORDER : UI_BORDER_COLOR);
    }
}

static void LCD_DrawMatrix4x4(uint16_t x, uint16_t y, uint16_t active_mask)
{
    uint8_t row;
    uint8_t col;
    uint8_t bit;
    uint8_t on;
    uint16_t px;
    uint16_t py;

    for (row = 0U; row < 4U; row++)
    {
        for (col = 0U; col < 4U; col++)
        {
            bit = (uint8_t)(row * 4U + col);
            on = ((active_mask & (uint16_t)(1U << bit)) != 0U) ? 1U : 0U;
            px = (uint16_t)(x + (uint16_t)col * 58U);
            py = (uint16_t)(y + (uint16_t)row * 42U);
            LCD_FillRounded(px, py, (uint16_t)(px + 44U), (uint16_t)(py + 30U), 5U,
                            on ? UI_WARN_ACCENT_COLOR : UI_CARD_ALT_BG);
            LCD_DrawRoundedRect(px, py, (uint16_t)(px + 44U), (uint16_t)(py + 30U), 5U, UI_BORDER_COLOR);
        }
    }
}

static void LCD_DrawSegmentH(uint16_t x, uint16_t y, uint16_t w, uint16_t t, uint32_t color)
{
    LCD_Fill((uint16_t)(x + t), y, (uint16_t)(x + w - t - 1U), (uint16_t)(y + t - 1U), color);
}

static void LCD_DrawSegmentV(uint16_t x, uint16_t y, uint16_t h, uint16_t t, uint32_t color)
{
    LCD_Fill(x, (uint16_t)(y + t), (uint16_t)(x + t - 1U), (uint16_t)(y + h - t - 1U), color);
}

static void LCD_DrawSevenSegDigit(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t value)
{
    static const uint8_t seg_mask[16] = {
        0x3FU, 0x06U, 0x5BU, 0x4FU, 0x66U, 0x6DU, 0x7DU, 0x07U,
        0x7FU, 0x6FU, 0x77U, 0x7CU, 0x39U, 0x5EU, 0x79U, 0x71U
    };
    const uint16_t t = 10U;
    const uint16_t half_h = (uint16_t)(h / 2U);
    uint8_t mask = seg_mask[value & 0x0FU];
    uint32_t color;

    LCD_FillRounded(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U), 8U, UI_DIGIT_BG);
    LCD_DrawRoundedRect(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U), 8U, UI_DIGIT_BORDER);

    color = ((mask & 0x01U) != 0U) ? UI_SEG_ON_COLOR : UI_SEG_OFF_COLOR;
    LCD_DrawSegmentH((uint16_t)(x + 6U), (uint16_t)(y + 6U), (uint16_t)(w - 12U), t, color);
    color = ((mask & 0x02U) != 0U) ? UI_SEG_ON_COLOR : UI_SEG_OFF_COLOR;
    LCD_DrawSegmentV((uint16_t)(x + w - t - 6U), (uint16_t)(y + 6U), half_h, t, color);
    color = ((mask & 0x04U) != 0U) ? UI_SEG_ON_COLOR : UI_SEG_OFF_COLOR;
    LCD_DrawSegmentV((uint16_t)(x + w - t - 6U), (uint16_t)(y + half_h), half_h, t, color);
    color = ((mask & 0x08U) != 0U) ? UI_SEG_ON_COLOR : UI_SEG_OFF_COLOR;
    LCD_DrawSegmentH((uint16_t)(x + 6U), (uint16_t)(y + h - t - 6U), (uint16_t)(w - 12U), t, color);
    color = ((mask & 0x10U) != 0U) ? UI_SEG_ON_COLOR : UI_SEG_OFF_COLOR;
    LCD_DrawSegmentV((uint16_t)(x + 6U), (uint16_t)(y + half_h), half_h, t, color);
    color = ((mask & 0x20U) != 0U) ? UI_SEG_ON_COLOR : UI_SEG_OFF_COLOR;
    LCD_DrawSegmentV((uint16_t)(x + 6U), (uint16_t)(y + 6U), half_h, t, color);
    color = ((mask & 0x40U) != 0U) ? UI_SEG_ON_COLOR : UI_SEG_OFF_COLOR;
    LCD_DrawSegmentH((uint16_t)(x + 6U), (uint16_t)(y + half_h - (t / 2U)), (uint16_t)(w - 12U), t, color);
}

static LcdMode0Status LCD_ParseMode0Status(uint32_t po_status, uint16_t pio_status)
{
    LcdMode0Status status = {
        .digit_word = po_status,
        .led_status = (uint16_t)(pio_status & 0x0FFFU)
    };

    return status;
}

static uint8_t LCD_MapMode7LogicalKey(uint8_t row_idx, uint8_t col_idx)
{
    static const uint8_t logical_key_map[4][4] = {
        {0xCU, 0xDU, 0xAU, 0xBU},
        {0x8U, 0x9U, 0xEU, 0xFU},
        {0x4U, 0x5U, 0x6U, 0x7U},
        {0x0U, 0x1U, 0x2U, 0x3U}
    };

    if ((row_idx >= 4U) || (col_idx >= 4U))
    {
        return 0U;
    }

    return logical_key_map[row_idx][col_idx];
}

static uint8_t LCD_MapElectricalToGridBit(uint8_t elec_row, uint8_t elec_col)
{
    static const uint8_t grid_bit_map[4][4] = {
        { 3U,  7U, 11U, 15U},
        { 9U, 10U, 12U, 14U},
        { 4U,  5U,  6U,  8U},
        {13U,  0U,  1U,  2U}
    };

    if ((elec_row >= 4U) || (elec_col >= 4U))
    {
        return 0U;
    }
    return grid_bit_map[elec_row][elec_col];
}

static LcdMode7Status LCD_ParseMode7Status(uint32_t po_status, uint16_t pio_status)
{
    LcdMode7Status status = {
        .key_value = 0U,
        .key_valid = (uint8_t)((pio_status >> 15U) & 0x0001U),
        .key_event = (uint8_t)((pio_status >> 14U) & 0x0001U),
        .multi_key = 0U,
        .key_pressed = 0U,
        .row_idx = (uint8_t)((pio_status >> 12U) & 0x0003U),
        .col_idx = (uint8_t)((pio_status >> 10U) & 0x0003U),
        .business_pio = (uint16_t)(pio_status & 0x03FFU),
        .business_po = po_status
    };

    status.key_pressed = status.key_valid;
    status.key_value = (status.key_valid != 0U) ? LCD_MapMode7LogicalKey(status.row_idx, status.col_idx) : 0U;

    return status;
}

static LcdMode8Status LCD_ParseMode8Status(uint32_t po_status, uint16_t pio_status)
{
    uint8_t digit;
    LcdMode8Status status = {
        .div_value = (uint16_t)((po_status >> 8U) & 0x0FFFUL),
        .tick_count = (uint8_t)((po_status >> 20U) & 0xFFUL),
        .step_select = (uint8_t)((po_status >> 31U) & 0x01UL),
        .clk_div2 = (uint8_t)((po_status >> 30U) & 0x01UL),
        .cw_pulse = (uint8_t)((po_status >> 29U) & 0x01UL),
        .ccw_pulse = (uint8_t)((po_status >> 28U) & 0x01UL),
        .led_status = (uint16_t)(pio_status & 0x0FFFU)
    };

    for (digit = 0U; digit < 8U; digit++)
    {
        status.display_nibbles[digit] = (uint8_t)((po_status >> ((uint32_t)digit * 4U)) & 0x0FUL);
    }

    return status;
}

static LcdMode9Status LCD_ParseMode9Status(uint32_t po_status, uint16_t pio_status)
{
    LcdMode9Status status = {
        .cursor_onehot = (uint8_t)(po_status & 0xFFUL),
        .cursor_index = (uint8_t)(pio_status & 0x0007U),
        .event_code = (uint8_t)((pio_status >> 3U) & 0x0007U),
        .boundary_hit = (uint8_t)((pio_status >> 6U) & 0x0001U),
        .key_mask = (uint8_t)((pio_status >> 7U) & 0x001FU)
    };

    return status;
}

static LcdModeAStatus LCD_ParseModeAStatus(uint32_t po_status, uint16_t pio_status)
{
    LcdModeAStatus status = {
        .hour = (uint8_t)((po_status >> 18U) & 0x3FUL),
        .minute = (uint8_t)((po_status >> 10U) & 0x3FUL),
        .second = (uint8_t)((po_status >> 2U) & 0x3FUL),
        .clock_state = (uint8_t)(po_status & 0x03UL),
        .edit_select = (uint8_t)((pio_status >> 2U) & 0x0003U),
        .tick_1hz = (uint8_t)((pio_status >> 4U) & 0x0001U),
        .control_status = (uint16_t)(pio_status & 0x0FFFU)
    };

    return status;
}

static void LCD_DrawMode0View(void)
{
    LcdMode0Status status = LCD_ParseMode0Status(current_xc7a_po, current_xc7a_pio);

    LCD_DrawMode0SevenSegView(status.digit_word);
    LCD_DrawMode0LedView(status.led_status);
}

static void LCD_DrawMode0SevenSegView(uint32_t po_value)
{
    uint8_t digit;
    uint8_t nibble;
    uint16_t x;
    const uint16_t y = 158U;
    const uint16_t digit_w = 72U;
    const uint16_t digit_h = 132U;
    const uint16_t gap = 16U;

    LCD_ShowChinese(32U, 140U, CH_DIGITS, 16U, UI_LABEL_COLOR, UI_CARD_BG);

    for (digit = 0U; digit < 8U; digit++)
    {
        nibble = (uint8_t)((po_value >> (28U - (uint32_t)digit * 4U)) & 0x0FU);
        x = (uint16_t)(32U + (uint16_t)digit * (digit_w + gap));

        LCD_DrawSevenSegDigit(x, y, digit_w, digit_h, nibble);
    }
}

static void LCD_DrawMode0LedView(uint16_t pio_value)
{
    uint8_t led;
    uint8_t led_on;
    uint16_t x;
    const uint16_t y = 310U;
    const uint16_t led_w = 42U;
    const uint16_t led_h = 34U;
    const uint16_t gap = 18U;

    /* Swap LED2 (bit 1) and LED4 (bit 3) to match FPGA led_board_logic_on */
    {
        uint16_t b1 = (pio_value >> 1) & 1U;
        uint16_t b3 = (pio_value >> 3) & 1U;
        pio_value &= ~((1U << 1) | (1U << 3));
        pio_value |= (b1 << 3) | (b3 << 1);
    }

    LCD_ShowChinese(32U, 292U, CH_LED_GROUP, 16U, UI_LABEL_COLOR, UI_CARD_BG);

    for (led = 0U; led < 12U; led++)
    {
        led_on = ((pio_value & (uint16_t)(1U << led)) != 0U) ? 1U : 0U;
        x = (uint16_t)(38U + (uint16_t)led * (led_w + gap));

        LCD_FillRounded(x, y, (uint16_t)(x + led_w - 1U),
                        (uint16_t)(y + led_h - 1U), 6U,
                        led_on ? UI_LED_ON_COLOR : UI_LED_OFF_COLOR);
        LCD_DrawRoundedRect(x, y, (uint16_t)(x + led_w - 1U),
                            (uint16_t)(y + led_h - 1U), 6U, UI_LED_BORDER);
        LCD_ShowFormatted((uint16_t)(x + 10U), (uint16_t)(y + 6U), 16U,
                          led_on ? LCD_COLOR_WHITE : UI_LABEL_COLOR,
                          led_on ? UI_LED_ON_COLOR : UI_LED_OFF_COLOR,
                          "L%02u", (unsigned int)(led + 1U));
    }
}

static void LCD_DrawMode1View(void)
{
    char result[16];
    char input_word[16];

    snprintf(result, sizeof(result), "%X %X %X %X",
             payload_nibbles[4], payload_nibbles[5], payload_nibbles[6], payload_nibbles[7]);
    snprintf(input_word, sizeof(input_word), "%04X", current_fmcu_pi);
    LCD_DrawValuePanelCN(44U, 146U, 344U, 236U, CH_RESULT, result, UI_WARN_ACCENT_COLOR);
    LCD_DrawValuePanelCN(388U, 146U, 734U, 236U, CH_KEY_CHAIN, input_word, UI_ACCENT_COLOR);
    LCD_DrawBitStrip(48U, 304U, 16U, current_fmcu_pi, CH_INPUT_BITS);
}

static void LCD_DrawMode2View(void)
{
    char decoded[8];
    char pattern[16];

    snprintf(decoded, sizeof(decoded), "%X", payload_nibbles[7]);
    snprintf(pattern, sizeof(pattern), "%02X", payload_bytes[3]);
    LCD_DrawValuePanelCN(54U, 148U, 340U, 248U, CH_DECODED_VALUE, decoded, UI_WARN_ACCENT_COLOR);
    LCD_DrawValuePanelCN(420U, 148U, 706U, 248U, CH_SEG_PATTERN, pattern, UI_ACCENT_COLOR);
    LCD_DrawNibbleRow(124U, 286U, 0U, 8U);
}

static void LCD_DrawMode3View(void)
{
    char key_value[16];

    snprintf(key_value, sizeof(key_value), "%02X", (unsigned int)(current_fmcu_pi & 0x00FFU));
    LCD_DrawPianoKeyStrip(92U, 132U, (uint8_t)(current_fmcu_pi & 0x00FFU));

    LCD_DrawValuePanelCN(54U, 278U, 734U, 328U, CH_LAST_EVENT, key_value, UI_ACCENT_COLOR);
}

static void LCD_DrawMode4View(void)
{
    char word[16];
    const uint8_t *dir_label = (current_xc7a_po & 0x00010000UL) ? CH_LEFT : CH_RIGHT;

    snprintf(word, sizeof(word), "%04lX", (unsigned long)(current_xc7a_po & 0xFFFFUL));
    LCD_DrawValuePanelCN(54U, 140U, 340U, 230U, CH_REGISTER, word, UI_WARN_ACCENT_COLOR);
    LCD_FillRounded(404U, 140U, 704U, 230U, 8U, UI_CARD_ALT_BG);
    LCD_DrawRoundedRect(404U, 140U, 704U, 230U, 8U, UI_BORDER_COLOR);
    LCD_ShowChinese(420U, 150U, CH_DIRECTION, 16U, UI_LABEL_COLOR, UI_CARD_ALT_BG);
    LCD_ShowChinese(420U, 174U, dir_label, 24U, UI_ACCENT_COLOR, UI_CARD_ALT_BG);
    LCD_DrawBitStrip(70U, 292U, 16U, current_xc7a_pio, CH_SHIFT_PATH);
}

static void LCD_DrawMode5View(void)
{
    char digit_value[16];
    char segment_value[16];
    char pio_value[16];

    snprintf(digit_value, sizeof(digit_value), "DIG:%02lX",
             (unsigned long)((current_xc7a_po >> 8U) & 0xFFUL));
    snprintf(segment_value, sizeof(segment_value), "SEG:%02lX",
             (unsigned long)(current_xc7a_po & 0xFFUL));
    snprintf(pio_value, sizeof(pio_value), "PIO:%03X",
             (unsigned int)(current_xc7a_pio & 0x0FFFU));

    LCD_DrawValuePanelCN(42U, 132U, 256U, 222U, CH_DIGITS, digit_value, UI_WARN_ACCENT_COLOR);
    LCD_DrawValuePanelCN(292U, 132U, 506U, 222U, CH_SEG_PATTERN, segment_value, UI_ACCENT_COLOR);
    LCD_DrawValuePanelCN(542U, 132U, 756U, 222U, CH_PIO_STATUS, pio_value, UI_ACCENT_COLOR);
    LCD_DrawBitStrip(76U, 306U, 12U, current_xc7a_pio, CH_PIO_STATUS);
}

static void LCD_DrawMode6View(void)
{
    char stable_value[20];
    char stretch_value[20];
    char toggle_value[20];
    char po_summary[20];
    const uint16_t stable_mask = (uint16_t)(current_xc7a_pio & 0x0049U);
    const uint16_t stretch_mask = (uint16_t)(current_xc7a_pio & 0x0092U);
    const uint16_t toggle_mask = (uint16_t)(current_xc7a_pio & 0x0024U);

    snprintf(stable_value, sizeof(stable_value), "PIO:%02X", (unsigned int)stable_mask);
    snprintf(stretch_value, sizeof(stretch_value), "PIO:%02X", (unsigned int)stretch_mask);
    snprintf(toggle_value, sizeof(toggle_value), "PIO:%02X", (unsigned int)toggle_mask);
    snprintf(po_summary, sizeof(po_summary), "PO:%02lX", (unsigned long)(current_xc7a_po & 0xFFUL));

    LCD_FillRounded(42U, 122U, 256U, 212U, 8U, UI_CARD_ALT_BG);
    LCD_DrawRoundedRect(42U, 122U, 256U, 212U, 8U, UI_BORDER_COLOR);
    LCD_ShowFormatted(58U, 134U, 16U, UI_LABEL_COLOR, UI_CARD_ALT_BG, "K1/K4/K7");
    LCD_ShowFormatted(58U, 158U, 16U, UI_ACCENT_COLOR, UI_CARD_ALT_BG, "STABLE");
    LCD_ShowFormatted(58U, 184U, 24U, UI_WARN_ACCENT_COLOR, UI_CARD_ALT_BG, "%s", stable_value);

    LCD_FillRounded(292U, 122U, 506U, 212U, 8U, UI_CARD_ALT_BG);
    LCD_DrawRoundedRect(292U, 122U, 506U, 212U, 8U, UI_BORDER_COLOR);
    LCD_ShowFormatted(308U, 134U, 16U, UI_LABEL_COLOR, UI_CARD_ALT_BG, "K2/K5/K8");
    LCD_ShowFormatted(308U, 158U, 16U, UI_ACCENT_COLOR, UI_CARD_ALT_BG, "STRETCH");
    LCD_ShowFormatted(308U, 184U, 24U, UI_WARN_ACCENT_COLOR, UI_CARD_ALT_BG, "%s", stretch_value);

    LCD_FillRounded(542U, 122U, 756U, 212U, 8U, UI_CARD_ALT_BG);
    LCD_DrawRoundedRect(542U, 122U, 756U, 212U, 8U, UI_BORDER_COLOR);
    LCD_ShowFormatted(558U, 134U, 16U, UI_LABEL_COLOR, UI_CARD_ALT_BG, "K3/K6");
    LCD_ShowFormatted(558U, 158U, 16U, UI_ACCENT_COLOR, UI_CARD_ALT_BG, "TOGGLE");
    LCD_ShowFormatted(558U, 184U, 24U, UI_WARN_ACCENT_COLOR, UI_CARD_ALT_BG, "%s", toggle_value);

    LCD_DrawValuePanelCN(54U, 226U, 340U, 286U, CH_SINGLE_PULSE, po_summary, UI_ACCENT_COLOR);
    LCD_DrawBitStrip(124U, 314U, 8U, current_xc7a_pio, CH_LCD_OUTPUT);
}

static void LCD_DrawMode7View(void)
{
    uint8_t row;
    uint8_t col;
    uint16_t label_y;
    uint16_t label_x;
    uint16_t active_mask = 0x0000U;
    char key_line[16];
    char row_col_line[16];
    char event_line[20];
    char bus_line[24];
    LcdMode7Status status = LCD_ParseMode7Status(current_xc7a_po, current_xc7a_pio);

    if ((status.key_valid != 0U) && (status.row_idx < 4U) && (status.col_idx < 4U))
    {
        uint8_t grid_bit = LCD_MapElectricalToGridBit(status.row_idx, status.col_idx);
        active_mask = (uint16_t)(1U << grid_bit);
    }

    snprintf(key_line, sizeof(key_line), "KEY:%X",
             (unsigned int)status.key_value);
    snprintf(row_col_line, sizeof(row_col_line), "ROW:%u COL:%u",
             (unsigned int)status.row_idx,
             (unsigned int)status.col_idx);
    snprintf(event_line, sizeof(event_line), "EDGE:%u ERR:%u",
             (unsigned int)status.key_event,
             (unsigned int)status.multi_key);
    snprintf(bus_line, sizeof(bus_line), "HOLD:%u PIO:%03X",
             (unsigned int)status.key_pressed,
             (unsigned int)status.business_pio);

    LCD_DrawValuePanelCN(54U, 120U, 254U, 188U, CH_CODE, key_line, UI_WARN_ACCENT_COLOR);
    LCD_DrawValuePanelCN(294U, 120U, 494U, 188U, CH_SCAN, row_col_line, UI_ACCENT_COLOR);
    LCD_DrawValuePanelCN(534U, 120U, 734U, 188U, CH_PIO_STATUS, event_line, UI_ACCENT_COLOR);
    LCD_DrawMatrix4x4(292U, 202U, active_mask);

    for (row = 0U; row < 4U; row++)
    {
        label_y = (uint16_t)(208U + (uint16_t)row * 42U);
        LCD_ShowFormatted(240U, label_y, 16U, UI_LABEL_COLOR, UI_CARD_BG,
                          "R%u", (unsigned int)row);
    }

    for (col = 0U; col < 4U; col++)
    {
        label_x = (uint16_t)(300U + (uint16_t)col * 58U);
        LCD_ShowFormatted(label_x, 338U, 16U, UI_LABEL_COLOR, UI_CARD_BG,
                          "C%u", (unsigned int)col);
    }

    LCD_ShowFormatted(548U, 208U, 16U, UI_LABEL_COLOR, UI_CARD_BG,
                      "VALID:%u", (unsigned int)status.key_valid);
    LCD_ShowFormatted(548U, 232U, 16U, UI_LABEL_COLOR, UI_CARD_BG,
                      "%s", bus_line);
    LCD_ShowFormatted(548U, 256U, 16U, UI_LABEL_COLOR, UI_CARD_BG,
                      "PO:%06lX", (unsigned long)status.business_po);
}

static void LCD_DrawMode8View(void)
{
    char param_value[20];
    char count_value[12];
    uint8_t digit;
    uint8_t nibble;
    uint16_t x;
    LcdMode8Status status = LCD_ParseMode8Status(current_xc7a_po, current_xc7a_pio);
    const uint16_t digit_y = 208U;
    const uint16_t digit_w = 56U;
    const uint16_t digit_h = 72U;
    const uint16_t gap = 30U;

    snprintf(param_value, sizeof(param_value), "%03X x%s",
             (unsigned int)status.div_value,
             (status.step_select != 0U) ? "16" : "1");
    snprintf(count_value, sizeof(count_value), "%02X %c%c",
             (unsigned int)status.tick_count,
             (status.cw_pulse != 0U) ? 'R' : '-',
             (status.ccw_pulse != 0U) ? 'L' : '-');

    LCD_DrawValuePanelCN(54U, 120U, 344U, 190U, CH_PARAM, param_value, UI_WARN_ACCENT_COLOR);
    LCD_DrawValuePanelCN(416U, 120U, 706U, 190U, CH_COUNT, count_value, UI_ACCENT_COLOR);
    LCD_ShowChinese(32U, 192U, CH_DIGITS, 16U, UI_LABEL_COLOR, UI_CARD_BG);

    for (digit = 0U; digit < 8U; digit++)
    {
        nibble = status.display_nibbles[digit];
        x = (uint16_t)(44U + (uint16_t)digit * (digit_w + gap));
        LCD_DrawSevenSegDigit(x, digit_y, digit_w, digit_h, nibble);
    }

    LCD_DrawMode0LedView(status.led_status);
}

static uint8_t LCD_GetMode9CursorPos(uint8_t cursor_onehot, uint8_t fallback_index)
{
    uint8_t bit;
    uint8_t onehot_count = 0U;
    uint8_t pos = 0U;

    for (bit = 0U; bit < 8U; bit++)
    {
        if ((cursor_onehot & (uint8_t)(1U << bit)) != 0U)
        {
            pos = bit;
            onehot_count++;
        }
    }

    if (onehot_count == 1U)
    {
        return pos;
    }

    return fallback_index;
}

static void LCD_DrawMode9View(void)
{
    char pio_summary[24];
    LcdMode9Status status = LCD_ParseMode9Status(current_xc7a_po, current_xc7a_pio);
    uint8_t current_pos = LCD_GetMode9CursorPos(status.cursor_onehot, status.cursor_index);
    uint8_t up_pressed = (uint8_t)(status.key_mask % 2U);
    uint8_t down_pressed = (uint8_t)((status.key_mask / 2U) % 2U);
    uint8_t left_pressed = (uint8_t)((status.key_mask / 4U) % 2U);
    uint8_t right_pressed = (uint8_t)((status.key_mask / 8U) % 2U);
    uint8_t center_pressed = (uint8_t)((status.key_mask / 16U) % 2U);
    uint32_t up_bg = up_pressed ? UI_STATUS_OK_BG : UI_CARD_ALT_BG;
    uint32_t down_bg = down_pressed ? UI_STATUS_OK_BG : UI_CARD_ALT_BG;
    uint32_t left_bg = left_pressed ? UI_STATUS_OK_BG : UI_CARD_ALT_BG;
    uint32_t right_bg = right_pressed ? UI_STATUS_OK_BG : UI_CARD_ALT_BG;
    uint32_t center_bg = center_pressed ? UI_STATUS_OK_BG : UI_CARD_ALT_BG;

    snprintf(pio_summary, sizeof(pio_summary), "POS:%u EVT:%u EDGE:%u",
             (unsigned int)current_pos,
             (unsigned int)status.event_code,
             (unsigned int)status.boundary_hit);

    LCD_DrawValuePanelCN(54U, 136U, 344U, 206U, CH_PIO_STATUS, pio_summary, UI_ACCENT_COLOR);

    LCD_FillRounded(482U, 128U, 592U, 178U, 8U, up_bg);
    LCD_DrawRoundedRect(482U, 128U, 592U, 178U, 8U, UI_BORDER_COLOR);
    LCD_ShowFormatted(520U, 142U, 24U, up_pressed ? LCD_COLOR_WHITE : UI_LABEL_COLOR, up_bg, "UP");

    LCD_FillRounded(482U, 242U, 592U, 292U, 8U, down_bg);
    LCD_DrawRoundedRect(482U, 242U, 592U, 292U, 8U, UI_BORDER_COLOR);
    LCD_ShowFormatted(508U, 256U, 24U, down_pressed ? LCD_COLOR_WHITE : UI_LABEL_COLOR, down_bg, "DOWN");

    LCD_FillRounded(354U, 185U, 464U, 235U, 8U, left_bg);
    LCD_DrawRoundedRect(354U, 185U, 464U, 235U, 8U, UI_BORDER_COLOR);
    LCD_ShowFormatted(382U, 199U, 24U, left_pressed ? LCD_COLOR_WHITE : UI_LABEL_COLOR, left_bg, "LEFT");

    LCD_FillRounded(610U, 185U, 720U, 235U, 8U, right_bg);
    LCD_DrawRoundedRect(610U, 185U, 720U, 235U, 8U, UI_BORDER_COLOR);
    LCD_ShowFormatted(630U, 199U, 24U, right_pressed ? LCD_COLOR_WHITE : UI_LABEL_COLOR, right_bg, "RIGHT");

    LCD_FillRounded(482U, 185U, 592U, 235U, 8U, center_bg);
    LCD_DrawRoundedRect(482U, 185U, 592U, 235U, 8U, UI_BORDER_COLOR);
    LCD_ShowFormatted(522U, 199U, 24U, center_pressed ? LCD_COLOR_WHITE : UI_LABEL_COLOR, center_bg, "OK");
}

static void LCD_DrawModeAView(void)
{
    char time_value[16];
    LcdModeAStatus status = LCD_ParseModeAStatus(current_xc7a_po, current_xc7a_pio);
    const uint8_t *ctrl_label = (status.clock_state == 0U) ? CH_RUN : CH_SET;

    snprintf(time_value, sizeof(time_value), "%02u:%02u:%02u",
             (unsigned int)status.hour,
             (unsigned int)status.minute,
             (unsigned int)status.second);
    LCD_FillRounded(54U, 132U, 474U, 242U, 8U, UI_CARD_ALT_BG);
    LCD_DrawRoundedRect(54U, 132U, 474U, 242U, 8U, UI_BORDER_COLOR);
    LCD_ShowChinese(86U, 148U, CH_TIME, 16U, UI_LABEL_COLOR, UI_CARD_ALT_BG);
    LCD_ShowFormatted(86U, 176U, 32U, UI_WARN_ACCENT_COLOR, UI_CARD_ALT_BG,
                      "%s", time_value);

    LCD_FillRounded(514U, 132U, 706U, 242U, 8U, UI_CARD_ALT_BG);
    LCD_DrawRoundedRect(514U, 132U, 706U, 242U, 8U, UI_BORDER_COLOR);
    LCD_ShowChinese(538U, 148U, CH_CONTROL, 16U, UI_LABEL_COLOR, UI_CARD_ALT_BG);
    LCD_ShowChinese(538U, 182U, ctrl_label, 24U, UI_ACCENT_COLOR, UI_CARD_ALT_BG);
    LCD_DrawBitStrip(76U, 310U, 12U, status.control_status, CH_CONTROL_KEYS);
}

static void LCD_DrawModeBView(void)
{
    char po_value[16];
    char pio_value[16];

    snprintf(po_value, sizeof(po_value), "%08lX", (unsigned long)current_xc7a_po);
    snprintf(pio_value, sizeof(pio_value), "%04X", current_xc7a_pio);
    LCD_DrawValuePanelCN(76U, 144U, 374U, 244U, CH_OUTPUT, po_value, UI_WARN_ACCENT_COLOR);
    LCD_DrawValuePanelCN(430U, 144U, 704U, 244U, CH_STATUS, pio_value, UI_ACCENT_COLOR);
    LCD_DrawBitStrip(94U, 314U, 12U, current_xc7a_pio, CH_LCD_OUTPUT);
}

static void LCD_DrawModeCView(void)
{
    char po_value[16];
    char pio_value[16];

    snprintf(po_value, sizeof(po_value), "%08lX", (unsigned long)current_xc7a_po);
    snprintf(pio_value, sizeof(pio_value), "%04X", current_xc7a_pio);
    LCD_DrawValuePanelCN(76U, 144U, 374U, 244U, CH_OUTPUT, po_value, UI_WARN_ACCENT_COLOR);
    LCD_DrawValuePanelCN(430U, 144U, 704U, 244U, CH_STATUS, pio_value, UI_ACCENT_COLOR);
    LCD_DrawBitStrip(94U, 314U, 12U, current_xc7a_pio, CH_LCD_OUTPUT);
}

static void LCD_UpdateStatusCard(uint32_t status_bg, uint32_t status_text)
{
    LCD_Fill(STATUS_X1, STATUS_Y1, STATUS_X2, STATUS_Y2, UI_HEADER_BG);
    LCD_DrawLine(0U, UI_HEADER_BOTTOM_Y, LCD_WIDTH - 1U, UI_HEADER_BOTTOM_Y, UI_BORDER_COLOR);
    LCD_ShowFormatted(HEADER_TITLE_X, HEADER_TITLE_Y, HEADER_TITLE_TEXT_SIZE, status_text, UI_HEADER_BG,
                      "M%X  %s", current_mode, get_mode_name(current_mode));
    LCD_ShowFormatted(HEADER_RESOURCE_X, HEADER_RESOURCE_Y, 16U, UI_HEADER_SUB, UI_HEADER_BG,
                      "%s", get_mode_resource_subtitle(current_mode));

    if (last_spi_status == HAL_OK)
    {
        LCD_ShowChinese(STATUS_LINK_LABEL_X, UI_STATUS_TEXT_Y, CH_LINK, 16U, status_bg, UI_HEADER_BG);
        LCD_ShowChinese(STATUS_LINK_VALUE_X, UI_STATUS_TEXT_Y, CH_READY, 16U, status_bg, UI_HEADER_BG);
    }
    else
    {
        LCD_ShowChinese(STATUS_LINK_LABEL_X, UI_STATUS_TEXT_Y, CH_LINK, 16U, status_bg, UI_HEADER_BG);
        LCD_ShowChinese(STATUS_LINK_VALUE_X, UI_STATUS_TEXT_Y, CH_LINK_DISCONNECT, 16U, status_bg, UI_HEADER_BG);
    }
}

static void LCD_UpdateDataCard(void)
{
    get_mode_ui_config(current_mode)->draw_view();
}

static void LCD_UpdateAuxCard(void)
{
    char freq_line[24];
    char mode_line[24];
    const ModeUiConfig *config = get_mode_ui_config(current_mode);
    uint8_t mode_text_size;

    snprintf(freq_line, sizeof(freq_line), "%s", get_frequency_name(current_clk_sel));
    snprintf(mode_line, sizeof(mode_line), "M%X %s", current_mode, get_mode_short_name(current_mode));
    mode_text_size = (config->compact_aux_mode_text != 0U) ? 16U : 24U;

    LCD_Fill(AUX_FREQ_VALUE_X1, AUX_FREQ_VALUE_Y1, AUX_FREQ_VALUE_X2, AUX_FREQ_VALUE_Y2,
             UI_CARD_CLOCK_BG);
    LCD_Fill(AUX_MODE_VALUE_X1, AUX_MODE_VALUE_Y1, AUX_MODE_VALUE_X2, AUX_MODE_VALUE_Y2,
             UI_CARD_CLOCK_BG);
    LCD_Fill(AUX_KEY_LED_VALUE_X1, AUX_KEY_LED_VALUE_Y1, AUX_KEY_LED_VALUE_X2, AUX_KEY_LED_VALUE_Y2,
             UI_CARD_CLOCK_BG);

    LCD_ShowChinese(AUX_FREQ_X, AUX_LABEL_Y, CH_FREQ, 16U, UI_LABEL_COLOR, UI_CARD_CLOCK_BG);
    LCD_ShowFormatted(AUX_FREQ_X, AUX_VALUE_Y, 32U, UI_WARN_ACCENT_COLOR, UI_CARD_CLOCK_BG,
                      "%s", freq_line);
    LCD_ShowChinese(AUX_MODE_X, AUX_LABEL_Y, CH_CUR_MODE, 16U, UI_LABEL_COLOR, UI_CARD_CLOCK_BG);
    LCD_ShowFormatted(AUX_MODE_X, 420U, mode_text_size, UI_VALUE_COLOR, UI_CARD_CLOCK_BG,
                      "%s", mode_line);
    if (mode_shows_key_led_strip(current_mode) != 0U)
    {
        LCD_DrawKeyLedStrip(current_key_led_mask);
    }
}

static void LCD_Display_Update(void)
{
    static uint8_t ui_cache_valid = 0U;
    static uint8_t prev_mode = 0U;
    static uint8_t prev_clk_sel = 0U;
    static HAL_StatusTypeDef prev_spi_status = HAL_OK;
    static uint16_t prev_rx_word0 = 0U;
    static uint16_t prev_rx_word1 = 0U;
    static uint16_t prev_rx_word2 = 0U;
    static uint16_t prev_fmcu_pi = 0U;
    static uint32_t prev_xc7a_po = 0U;
    static uint16_t prev_xc7a_pio = 0U;
    static uint8_t prev_key_led_mask = 0U;
    uint8_t lcd_output_mode;
    uint8_t key_chain_mode;
    uint8_t mode_dirty;
    uint8_t clk_dirty;
    uint8_t status_dirty;
    uint8_t link_state_dirty;
    uint8_t legacy_dirty;
    uint8_t po_dirty;
    uint8_t pio_dirty;
    uint8_t key_led_dirty;
    uint8_t layout_dirty;
    uint8_t data_dirty;
    uint8_t aux_dirty;
    uint32_t status_bg;
    uint32_t status_text = LCD_COLOR_WHITE;

    lcd_output_mode = mode_has_lcd_output(current_mode);
    key_chain_mode = mode_uses_key_chain_display(current_mode);
    mode_dirty = ((ui_cache_valid == 0U) || (prev_mode != current_mode)) ? 1U : 0U;
    clk_dirty = ((ui_cache_valid == 0U) || (prev_clk_sel != current_clk_sel)) ? 1U : 0U;
    status_dirty = ((ui_cache_valid == 0U) || (prev_spi_status != last_spi_status) || (mode_dirty != 0U)) ? 1U : 0U;
    link_state_dirty = ((ui_cache_valid == 0U) ||
                        ((prev_spi_status == HAL_OK) != (last_spi_status == HAL_OK))) ? 1U : 0U;
    legacy_dirty = ((prev_rx_word0 != rx_word0) ||
                    (prev_rx_word1 != rx_word1) ||
                    (prev_rx_word2 != rx_word2) ||
                    (prev_fmcu_pi != current_fmcu_pi)) ? 1U : 0U;
    po_dirty = (prev_xc7a_po != current_xc7a_po) ? 1U : 0U;
    pio_dirty = (prev_xc7a_pio != current_xc7a_pio) ? 1U : 0U;
    key_led_dirty = ((ui_cache_valid == 0U) || (prev_key_led_mask != current_key_led_mask)) ? 1U : 0U;
    layout_dirty = ((ui_cache_valid == 0U) || (mode_dirty != 0U) || (link_state_dirty != 0U)) ? 1U : 0U;
    data_dirty = ((layout_dirty != 0U) ||
                  ((key_chain_mode != 0U) && (legacy_dirty != 0U)) ||
                  ((lcd_output_mode != 0U) &&
                   ((legacy_dirty != 0U) || (po_dirty != 0U) || (pio_dirty != 0U)))) ? 1U : 0U;
    aux_dirty = ((layout_dirty != 0U) || (clk_dirty != 0U) || (status_dirty != 0U) || (key_led_dirty != 0U)) ? 1U : 0U;

    if ((ui_cache_valid != 0U) &&
        (status_dirty == 0U) &&
        (data_dirty == 0U) &&
        (aux_dirty == 0U) &&
        ((lcd_output_mode == 0U) || ((po_dirty == 0U) && (pio_dirty == 0U))))
    {
        return;
    }

    ui_cache_valid = 1U;
    prev_mode = current_mode;
    prev_clk_sel = current_clk_sel;
    prev_spi_status = last_spi_status;
    prev_rx_word0 = rx_word0;
    prev_rx_word1 = rx_word1;
    prev_rx_word2 = rx_word2;
    prev_fmcu_pi = current_fmcu_pi;
    prev_xc7a_po = current_xc7a_po;
    prev_xc7a_pio = current_xc7a_pio;
    prev_key_led_mask = current_key_led_mask;

    if (last_spi_status == HAL_OK)
    {
        status_bg = UI_STATUS_OK_BG;
    }
    else
    {
        status_bg = UI_STATUS_ERR_BG;
    }

    if (layout_dirty != 0U)
    {
        LCD_ClearDynamicRegions();
    }

    if (status_dirty != 0U)
    {
        LCD_UpdateStatusCard(status_bg, status_text);
        LCD_RefreshRect(STATUS_X1, STATUS_Y1, STATUS_X2, STATUS_Y2);
    }

    if (data_dirty != 0U)
    {
        LCD_UpdateDataCard();
        LCD_RefreshRect((uint16_t)(DATA_X1 + 2U), (uint16_t)(DATA_Y1 + UI_CARD_TITLE_H + 2U),
                        (uint16_t)(DATA_X2 - 2U), (uint16_t)(DATA_Y2 - 2U));
    }

    if (aux_dirty != 0U)
    {
        LCD_UpdateAuxCard();
        LCD_RefreshRect((uint16_t)(CLOCK_X1 + 2U), (uint16_t)(CLOCK_Y1 + UI_CARD_TITLE_H + 2U),
                        (uint16_t)(CLOCK_X2 - 2U), (uint16_t)(CLOCK_Y2 - 2U));
    }
}

static void SDRAM_BasicTest(void)
{
    const uint32_t framebuffer_bytes = (uint32_t)LCD_RGB_WIDTH * (uint32_t)LCD_RGB_HEIGHT * sizeof(uint16_t);
    if (bsp_TestExtSDRAM_Block(LCD_RGB_FB_ADDR, framebuffer_bytes) != 0U)
    {
        Error_Handler();
    }
}

static void Framebuffer_Init(void)
{
    LCD_FillFramebufferColor(0x0000U);
}

static void LCD_FillFramebufferColor(uint16_t rgb565)
{
    uint32_t i;
    uint16_t *framebuffer = (uint16_t *)LCD_RGB_FB_ADDR;
    const uint32_t total_pixels = LCD_RGB_WIDTH * LCD_RGB_HEIGHT;

    for (i = 0U; i < total_pixels; i++)
    {
        framebuffer[i] = rgb565;
    }

    SCB_CleanDCache_by_Addr((uint32_t *)LCD_RGB_FB_ADDR,
                            (int32_t)(total_pixels * sizeof(uint16_t)));
}

static void LCD_DiagnosticTest(void)
{
    while (1)
    {
        LCD_FillFramebufferColor(0xF800U);
        HAL_Delay(1000U);

        LCD_FillFramebufferColor(0x07E0U);
        HAL_Delay(1000U);

        LCD_FillFramebufferColor(0x001FU);
        HAL_Delay(1000U);

        LCD_FillFramebufferColor(0xFFFFU);
        HAL_Delay(1000U);
    }
}

const char *get_mode_name(uint8_t mode)
{
    switch (mode)
    {
        case 0x0: return "RESULT DISPLAY";
        case 0x1: return "INPUT MAPPING";
        case 0x2: return "DECODER SELECT";
        case 0x3: return "EVENT TRIGGER";
        case 0x4: return "PARALLEL INPUT";
        case 0x5: return "SCAN DISPLAY";
        case 0x6: return "SIGNAL SHAPING";
        case 0x7: return "MATRIX KEYS";
        case 0x8: return "ROTARY CONTROL";
        case 0x9: return "NAV CONTROL";
        case 0xA: return "CONTROL PANEL";
        case 0xB: return "INSTRUMENT VIEW";
        case 0xC: return "REMOTE CONTROL";
        default: return "UNKNOWN";
    }
}

const char *get_mode_short_name(uint8_t mode)
{
    switch (mode)
    {
        case 0x7: return "KEYPAD";
        case 0x8: return "ENCODER";
        case 0x9: return "5WAY";
        case 0xA: return "CLOCK";
        default: return get_mode_name(mode);
    }
}

const char *get_frequency_name(uint8_t clk_sel)
{
    static const char * const freq_names[] = {
        "0.5Hz", "1Hz",   "2Hz",   "4Hz",   "8Hz",    "16Hz",   "32Hz",
        "64Hz",  "128Hz", "256Hz", "512Hz", "1kHz",   "2kHz",   "4kHz",
        "16kHz", "32kHz", "64kHz", "625kHz","750kHz", "1.25MHz","2.5MHz",
        "3MHz",  "5MHz",  "6MHz",  "10MHz", "12MHz",  "20MHz",  "24MHz"
    };

    if (clk_sel < (sizeof(freq_names) / sizeof(freq_names[0])))
    {
        return freq_names[clk_sel];
    }

    return "UNKNOWN";
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    MODIFY_REG(PWR->CR3, PWR_CR3_SCUEN, 0);
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY))
    {
    }

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 5;
    RCC_OscInitStruct.PLL.PLLN = 160;
    RCC_OscInitStruct.PLL.PLLP = 2;
    RCC_OscInitStruct.PLL.PLLQ = 4;
    RCC_OscInitStruct.PLL.PLLR = 2;
    RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
    RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    RCC_OscInitStruct.PLL.PLLFRACN = 0;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 |
                                  RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
    RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
    {
        Error_Handler();
    }

    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LTDC | RCC_PERIPHCLK_SPI123;
    PeriphClkInitStruct.Spi123ClockSelection = RCC_SPI123CLKSOURCE_PLL;
    PeriphClkInitStruct.PLL3.PLL3M = 5;
    PeriphClkInitStruct.PLL3.PLL3N = 88;
    PeriphClkInitStruct.PLL3.PLL3P = 2;
    PeriphClkInitStruct.PLL3.PLL3Q = 4;
    PeriphClkInitStruct.PLL3.PLL3R = 18;
    PeriphClkInitStruct.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_2;
    PeriphClkInitStruct.PLL3.PLL3VCOSEL = RCC_PLL3VCOWIDE;
    PeriphClkInitStruct.PLL3.PLL3FRACN = 0;

    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    __HAL_RCC_CSI_ENABLE();
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    HAL_EnableCompensationCell();
}

void MPU_Config(void)
{
    MPU_Region_InitTypeDef MPU_InitStruct = {0};

    HAL_MPU_Disable();

    MPU_InitStruct.Enable = MPU_REGION_ENABLE;
    MPU_InitStruct.Number = MPU_REGION_NUMBER0;
    MPU_InitStruct.BaseAddress = 0x24000000U;
    MPU_InitStruct.Size = MPU_REGION_SIZE_512KB;
    MPU_InitStruct.SubRegionDisable = 0x00U;
    MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL1;
    MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
    MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
    MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
    MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
    MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;
    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    MPU_InitStruct.Number = MPU_REGION_NUMBER1;
    MPU_InitStruct.BaseAddress = 0x60000000U;
    MPU_InitStruct.Size = ARM_MPU_REGION_SIZE_64KB;
    MPU_InitStruct.SubRegionDisable = 0x00U;
    MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
    MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
    MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
    MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
    MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
    MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;
    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    MPU_InitStruct.Number = MPU_REGION_NUMBER2;
    MPU_InitStruct.BaseAddress = EXT_SDRAM_ADDR;
    MPU_InitStruct.Size = MPU_REGION_SIZE_32MB;
    MPU_InitStruct.SubRegionDisable = 0x00U;
    MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
    MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
    MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
    MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
    MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
    MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

void CPU_CACHE_Enable(void)
{
    SCB_EnableICache();
    SCB_EnableDCache();
}

void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}

void SysTick_Handler(void)
{
    HAL_IncTick();
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}
#endif

