#include "fpga_config.h"

FPGA_StateTypeDef g_fpga_state = FPGA_STATE_IDLE;
JtagHalCallbacks jtag_hal_ops;
uint8_t g_fpga_config_start = 0;
uint8_t g_xsvf_exec_start = 0;

uint8_t INIT_STATE = 0;
uint8_t PROG_STATE = 0;
uint8_t DONE_STATE = 0;
uint32_t send_total = 0;

#define JTAG_GPIO_SET(port, pin)   ((port)->BSRR = (pin))
#define JTAG_GPIO_CLR(port, pin)   ((port)->BSRR = ((uint32_t)(pin) << 16U))
#define JTAG_GPIO_WRITE(port, pin, level) \
    do { \
        if (level) { \
            JTAG_GPIO_SET((port), (pin)); \
        } else { \
            JTAG_GPIO_CLR((port), (pin)); \
        } \
    } while (0)
#define JTAG_TDO_IS_HIGH()         ((JTAG_GPIO_PORT->IDR & JTAG_TDO_PIN) != 0U)
#define JTAG_EDGE_DELAY_NS         0U

static HAL_StatusTypeDef FPGA_Wait_InitB_Ready(void);
static HAL_StatusTypeDef FPGA_Wait_DONE_High(void);
static void BSP_Jtag_SetTms(bool level);
static void BSP_Jtag_SetTck(bool level);
static void BSP_Jtag_SetTdi(bool level);
static bool BSP_Jtag_ReadTdo(void);
static void Jtag_Tick(JtagContext *jtag, bool tms, bool tdi);
static void Jtag_ShiftRawOrder(JtagContext *jtag, const uint8_t *tx_data, uint8_t *rx_data,
                               uint32_t bit_len, bool end_after_shift, bool lsb_first,
                               bool default_tdi_high);
static void Jtag_ShiftCfgInFast(JtagContext *jtag, const uint8_t *tx_data, uint32_t byte_len,
                                bool end_after_shift);
static HAL_StatusTypeDef Jtag_WaitInitBHigh(JtagContext *jtag, uint32_t timeout_ms);
static bool Jtag_ShiftXsvf(JtagContext *jtag, const uint8_t *tx_data, uint8_t *rx_data,
                           uint32_t bit_len, bool is_ir, JtagState end_state);
static bool Xsvf_ReadU8(const uint8_t **cursor, const uint8_t *end, uint8_t *value);
static bool Xsvf_ReadU32BE(const uint8_t **cursor, const uint8_t *end, uint32_t *value);
static bool Xsvf_ReadBytes(const uint8_t **cursor, const uint8_t *end, uint8_t *dst, uint32_t len);
static JtagState Xsvf_MapState(uint8_t xsvf_state, bool ir_path);
static HAL_StatusTypeDef Xsvf_RunTest(JtagContext *jtag, uint32_t clocks_and_us);
static bool Xsvf_CompareMasked(const uint8_t *actual, const uint8_t *expected,
                               const uint8_t *mask, uint32_t len, bool use_mask);

void FPGA_Delay_NS(uint32_t ns)
{
    if (ns == 0U) {
        return;
    }

    static uint8_t dwt_init = 0U;
    if (dwt_init == 0U) {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CYCCNT = 0U;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
        dwt_init = 1U;
    }

    uint64_t total_cycles = (uint64_t)ns * SystemCoreClock / 1000000000ULL;
    uint32_t target = DWT->CYCCNT + (uint32_t)total_cycles;

    if (target < DWT->CYCCNT) {
        while (DWT->CYCCNT > target) {
        }
    }

    while (DWT->CYCCNT < target) {
    }
}

static void BSP_Jtag_SetTms(bool level)
{
    JTAG_GPIO_WRITE(JTAG_GPIO_PORT, JTAG_TMS_PIN, level);
}

static void BSP_Jtag_SetTck(bool level)
{
    JTAG_GPIO_WRITE(JTAG_GPIO_PORT, JTAG_TCK_PIN, level);
}

static void BSP_Jtag_SetTdi(bool level)
{
    JTAG_GPIO_WRITE(JTAG_GPIO_PORT, JTAG_TDI_PIN, level);
}

static bool BSP_Jtag_ReadTdo(void)
{
    return JTAG_TDO_IS_HIGH();
}

static HAL_StatusTypeDef FPGA_Wait_InitB_Ready(void)
{
    g_fpga_state = FPGA_STATE_WAIT_INITB;

    uint32_t timeout = 10000U;
    INIT_STATE = HAL_GPIO_ReadPin(FPGA_INITB_PORT, FPGA_INITB_PIN);
    PROG_STATE = HAL_GPIO_ReadPin(FPGA_PROGB_PORT, FPGA_PROGB_PIN);

    while (HAL_GPIO_ReadPin(FPGA_INITB_PORT, FPGA_INITB_PIN) == GPIO_PIN_RESET) {
        osDelay(1);
        if (--timeout == 0U) {
            g_fpga_state = FPGA_STATE_FAILED;
            return HAL_TIMEOUT;
        }
    }

    return HAL_OK;
}

static HAL_StatusTypeDef FPGA_Wait_DONE_High(void)
{
    g_fpga_state = FPGA_STATE_WAIT_DONE;

    uint32_t timeout = 2000U;
    DONE_STATE = HAL_GPIO_ReadPin(FPGA_DONE_PORT, FPGA_DONE_PIN);

    while (HAL_GPIO_ReadPin(FPGA_DONE_PORT, FPGA_DONE_PIN) == GPIO_PIN_RESET) {
        osDelay(1);

        if (--timeout == 0U) {
            g_fpga_state = FPGA_STATE_FAILED;
            return HAL_TIMEOUT;
        }

        if (HAL_GPIO_ReadPin(FPGA_INITB_PORT, FPGA_INITB_PIN) == GPIO_PIN_RESET) {
            xTaskResumeAll();
            CDC_Transmit_FS((uint8_t *)"[FPGA] CRC Error! INIT_B dropped (Wait DONE)\r\n", 48);
            g_fpga_state = FPGA_STATE_FAILED;
            return HAL_ERROR;
        }
    }

    return HAL_OK;
}

HAL_StatusTypeDef FPGA_Send_Bin_From_SDRAM(uint32_t bin_size)
{
    if ((bin_size == 0U) || (bin_size > SDRAM_TOTAL_SIZE)) {
        CDC_Transmit_FS((uint8_t *)"[FPGA] Invalid bin size\r\n", 24);
        return HAL_ERROR;
    }

    send_total = 0U;

    FPGA_Reset();
    CDC_Transmit_FS((uint8_t *)"[FPGA] FPGA Reset OK\r\n", 22);

    osDelay(1);

    if (FPGA_Wait_InitB_Ready() != HAL_OK) {
        CDC_Transmit_FS((uint8_t *)"[FPGA] INIT_B timeout\r\n", 24);
        osDelay(10);
        return HAL_TIMEOUT;
    }

    if (HAL_GPIO_ReadPin(FPGA_INITB_PORT, FPGA_INITB_PIN) != GPIO_PIN_SET) {
        CDC_Transmit_FS((uint8_t *)"[FPGA] INIT_B abnormal after ready check\r\n", 43);
        g_fpga_state = FPGA_STATE_FAILED;
        return HAL_ERROR;
    }

    CDC_Transmit_FS((uint8_t *)"[FPGA] INIT_B ready\r\n", 22);
    osDelay(1);

    g_fpga_state = FPGA_STATE_SENDING;
    CDC_Transmit_FS((uint8_t *)"[FPGA] Sending bin data via SPI+DMA (one-shot)...\r\n", 52);
    osDelay(10);

    uint8_t *p_sdram = (uint8_t *)SDRAM_BASE_ADDR;
    uint32_t batch_size = 4096U;

    while (send_total < bin_size) {
        if (HAL_GPIO_ReadPin(FPGA_INITB_PORT, FPGA_INITB_PIN) == GPIO_PIN_RESET) {
            CDC_Transmit_FS((uint8_t *)"[FPGA] ERROR: INIT_B dropped before send!\r\n", 44);
            g_fpga_state = FPGA_STATE_FAILED;
            return HAL_ERROR;
        }

        uint32_t current_batch = ((bin_size - send_total) > batch_size) ? batch_size : (bin_size - send_total);

        if (HAL_SPI_Transmit_DMA(&hspi4, &p_sdram[send_total], current_batch) != HAL_OK) {
            CDC_Transmit_FS((uint8_t *)"[FPGA] ERROR: DMA transmit failed!\r\n", 36);
            g_fpga_state = FPGA_STATE_FAILED;
            HAL_SPI_Abort(&hspi4);
            return HAL_ERROR;
        }

        while (HAL_SPI_GetState(&hspi4) != HAL_SPI_STATE_READY) {
            if (HAL_GPIO_ReadPin(FPGA_INITB_PORT, FPGA_INITB_PIN) == GPIO_PIN_RESET) {
                HAL_SPI_Abort(&hspi4);
                CDC_Transmit_FS((uint8_t *)"[FPGA] ERROR: INIT_B dropped (DMA abort)!\r\n", 44);
                g_fpga_state = FPGA_STATE_FAILED;
                return HAL_ERROR;
            }
        }

        send_total += current_batch;
    }

    {
        uint8_t dummy_data[100] = {0};
        HAL_SPI_Transmit_DMA(&hspi4, dummy_data, sizeof(dummy_data));
        while (HAL_SPI_GetState(&hspi4) != HAL_SPI_STATE_READY) {
        }
    }

    if (FPGA_Wait_DONE_High() != HAL_OK) {
        CDC_Transmit_FS((uint8_t *)"[FPGA] DONE timeout\r\n", 22);
        return HAL_TIMEOUT;
    }

    g_fpga_state = FPGA_STATE_SUCCESS;
    CDC_Transmit_FS((uint8_t *)"[FPGA] Config complete\r\n", 24);
    osDelay(10);

    return HAL_OK;
}

void FPGA_Reset(void)
{
    g_fpga_state = FPGA_STATE_RESET;
    HAL_GPIO_WritePin(FPGA_PROGB_PORT, FPGA_PROGB_PIN, GPIO_PIN_RESET);
    osDelay(10);
    HAL_GPIO_WritePin(FPGA_PROGB_PORT, FPGA_PROGB_PIN, GPIO_PIN_SET);
    osDelay(10);
    g_fpga_state = FPGA_STATE_WAIT_INITB;
}

void FPGA_Check_Config_Cmd(uint8_t *buf, uint32_t len)
{
    static uint8_t g_cfg_cmd_buf[2] = {0};
    static uint8_t g_cfg_cmd_idx = 0U;

    for (uint32_t i = 0; i < len; i++) {
        if ((g_cfg_cmd_idx == 0U) && (buf[i] == CMD_START_FPGA_CONFIG_BYTE1)) {
            g_cfg_cmd_buf[g_cfg_cmd_idx++] = buf[i];
        } else if (g_cfg_cmd_idx == 1U) {
            g_cfg_cmd_buf[g_cfg_cmd_idx++] = buf[i];

            if ((g_cfg_cmd_buf[0] == CMD_START_FPGA_CONFIG_BYTE1) &&
                (g_cfg_cmd_buf[1] == CMD_START_FPGA_CONFIG_BYTE2)) {
                g_fpga_config_start = 1U;
                CDC_Transmit_FS((uint8_t *)"[FPGA] Start config cmd received\r\n", 34);
                g_cfg_cmd_idx = 0U;
                break;
            } else if ((g_cfg_cmd_buf[0] == CMD_START_XSVF_EXEC_BYTE1) &&
                       (g_cfg_cmd_buf[1] == CMD_START_XSVF_EXEC_BYTE2)) {
                g_xsvf_exec_start = 1U;
                CDC_Transmit_FS((uint8_t *)"[XSVF] Start execute cmd received\r\n", 36);
                g_cfg_cmd_idx = 0U;
                break;
            } else {
                g_cfg_cmd_idx = 0U;
            }
        } else {
            g_cfg_cmd_idx = 0U;
        }
    }
}

static const uint8_t jtag_tms_lut[JTAG_STATE_COUNT][JTAG_STATE_COUNT] = {
    {0x00, 0x00, 0x02, 0x02, 0x02, 0x0A, 0x0A, 0x2A, 0x1A, 0x06, 0x06, 0x06, 0x16, 0x16, 0x56, 0x36},
    {0x07, 0x00, 0x01, 0x01, 0x01, 0x05, 0x05, 0x15, 0x0D, 0x03, 0x03, 0x03, 0x0B, 0x0B, 0x2B, 0x1B},
    {0x03, 0x03, 0x00, 0x00, 0x00, 0x02, 0x02, 0x0A, 0x06, 0x01, 0x01, 0x01, 0x05, 0x05, 0x15, 0x0D},
    {0x1F, 0x03, 0x07, 0x00, 0x00, 0x01, 0x01, 0x05, 0x03, 0x0F, 0x0F, 0x0F, 0x2F, 0x2F, 0xAF, 0x6F},
    {0x1F, 0x03, 0x07, 0x07, 0x00, 0x01, 0x01, 0x05, 0x03, 0x0F, 0x0F, 0x0F, 0x2F, 0x2F, 0xAF, 0x6F},
    {0x0F, 0x01, 0x03, 0x03, 0x02, 0x00, 0x00, 0x02, 0x01, 0x07, 0x07, 0x07, 0x17, 0x17, 0x57, 0x37},
    {0x1F, 0x03, 0x07, 0x07, 0x01, 0x05, 0x00, 0x01, 0x03, 0x0F, 0x0F, 0x0F, 0x2F, 0x2F, 0xAF, 0x6F},
    {0x0F, 0x01, 0x03, 0x03, 0x00, 0x02, 0x02, 0x00, 0x01, 0x07, 0x07, 0x07, 0x17, 0x17, 0x57, 0x37},
    {0x07, 0x00, 0x01, 0x01, 0x01, 0x05, 0x05, 0x15, 0x00, 0x03, 0x03, 0x03, 0x0B, 0x0B, 0x2B, 0x1B},
    {0x01, 0x01, 0x05, 0x05, 0x05, 0x15, 0x15, 0x55, 0x35, 0x00, 0x00, 0x00, 0x02, 0x02, 0x0A, 0x06},
    {0x1F, 0x03, 0x07, 0x07, 0x07, 0x17, 0x17, 0x57, 0x37, 0x0F, 0x00, 0x00, 0x01, 0x01, 0x05, 0x03},
    {0x1F, 0x03, 0x07, 0x07, 0x07, 0x17, 0x17, 0x57, 0x37, 0x0F, 0x0F, 0x00, 0x01, 0x01, 0x05, 0x03},
    {0x0F, 0x01, 0x03, 0x03, 0x03, 0x0B, 0x0B, 0x2B, 0x1B, 0x07, 0x07, 0x02, 0x00, 0x00, 0x02, 0x01},
    {0x1F, 0x03, 0x07, 0x07, 0x07, 0x17, 0x17, 0x57, 0x37, 0x0F, 0x0F, 0x01, 0x05, 0x00, 0x01, 0x03},
    {0x0F, 0x01, 0x03, 0x03, 0x03, 0x0B, 0x0B, 0x2B, 0x1B, 0x07, 0x07, 0x00, 0x02, 0x02, 0x00, 0x01},
    {0x07, 0x00, 0x01, 0x01, 0x01, 0x05, 0x05, 0x15, 0x0D, 0x03, 0x03, 0x03, 0x0B, 0x0B, 0x2B, 0x00}
};

static const uint8_t jtag_steps_lut[JTAG_STATE_COUNT][JTAG_STATE_COUNT] = {
    {0, 1, 2, 3, 4, 4, 5, 6, 5, 3, 4, 5, 5, 6, 7, 6},
    {3, 0, 1, 2, 3, 3, 4, 5, 4, 2, 3, 4, 4, 5, 6, 5},
    {2, 3, 0, 1, 2, 2, 3, 4, 3, 1, 2, 3, 3, 4, 5, 4},
    {5, 3, 3, 0, 1, 1, 2, 3, 2, 4, 5, 6, 6, 7, 8, 7},
    {5, 3, 3, 4, 0, 1, 2, 3, 2, 4, 5, 6, 6, 7, 8, 7},
    {4, 2, 2, 3, 3, 0, 1, 2, 1, 3, 4, 5, 5, 6, 7, 6},
    {5, 3, 3, 4, 2, 3, 0, 1, 2, 4, 5, 6, 6, 7, 8, 7},
    {4, 2, 2, 3, 1, 2, 3, 0, 1, 3, 4, 5, 5, 6, 7, 6},
    {3, 1, 1, 2, 3, 3, 4, 5, 0, 2, 3, 4, 4, 5, 6, 5},
    {1, 2, 3, 4, 5, 5, 6, 7, 6, 0, 1, 2, 2, 3, 4, 3},
    {5, 3, 3, 4, 5, 5, 6, 7, 6, 4, 0, 1, 1, 2, 3, 2},
    {5, 3, 3, 4, 5, 5, 6, 7, 6, 4, 5, 0, 1, 2, 3, 2},
    {4, 2, 2, 3, 4, 4, 5, 6, 5, 3, 4, 3, 0, 1, 2, 1},
    {5, 3, 3, 4, 5, 5, 6, 7, 6, 4, 5, 2, 3, 0, 1, 2},
    {4, 2, 2, 3, 4, 4, 5, 6, 5, 3, 4, 1, 2, 3, 0, 1},
    {3, 1, 1, 2, 3, 3, 4, 5, 4, 2, 3, 4, 4, 5, 6, 0}
};

static void Jtag_Tick(JtagContext *jtag, bool tms, bool tdi)
{
    (void)jtag;

    JTAG_GPIO_WRITE(JTAG_GPIO_PORT, JTAG_TMS_PIN, tms);
    JTAG_GPIO_WRITE(JTAG_GPIO_PORT, JTAG_TDI_PIN, tdi);

    if (JTAG_EDGE_DELAY_NS != 0U) {
        FPGA_Delay_NS(JTAG_EDGE_DELAY_NS);
    }

    JTAG_GPIO_SET(JTAG_GPIO_PORT, JTAG_TCK_PIN);

    if (JTAG_EDGE_DELAY_NS != 0U) {
        FPGA_Delay_NS(JTAG_EDGE_DELAY_NS);
    }

    JTAG_GPIO_CLR(JTAG_GPIO_PORT, JTAG_TCK_PIN);
}

void Jtag_Init(JtagContext *jtag, JtagHalCallbacks *hal, JtagTransfer *xfer)
{
    jtag->hal = hal;
    jtag->xfer = xfer;
    jtag->current_state = JTAG_TLR;
    jtag->last_bit_held = false;
    jtag->last_bit = 0U;

    JTAG_GPIO_CLR(JTAG_GPIO_PORT, JTAG_TCK_PIN);
    JTAG_GPIO_SET(JTAG_GPIO_PORT, JTAG_TMS_PIN);
    JTAG_GPIO_SET(JTAG_GPIO_PORT, JTAG_TDI_PIN);

    Jtag_Reset(jtag);
}

void Jtag_Reset(JtagContext *jtag)
{
    for (int i = 0; i < 5; i++) {
        Jtag_Tick(jtag, true, true);
    }

    jtag->current_state = JTAG_TLR;
}

void Jtag_GotoState(JtagContext *jtag, JtagState target_state)
{
    if (jtag->current_state == target_state) {
        return;
    }

    uint8_t tms_seq = jtag_tms_lut[jtag->current_state][target_state];
    uint8_t steps = jtag_steps_lut[jtag->current_state][target_state];

    while (steps--) {
        bool tms = (tms_seq & 0x01U) != 0U;
        Jtag_Tick(jtag, tms, true);
        tms_seq >>= 1;
    }

    jtag->current_state = target_state;
}

void Jtag_RunClocks(JtagContext *jtag, uint32_t count, JtagState end_state)
{
    Jtag_GotoState(jtag, JTAG_RTI);

    while (count--) {
        Jtag_Tick(jtag, false, true);
    }

    Jtag_GotoState(jtag, end_state);
}

static void Jtag_ShiftRawOrder(JtagContext *jtag, const uint8_t *tx_data, uint8_t *rx_data,
                               uint32_t bit_len, bool end_after_shift, bool lsb_first,
                               bool default_tdi_high)
{
    if ((bit_len == 0U) || (jtag == NULL)) {
        return;
    }

    if (rx_data != NULL) {
        memset(rx_data, 0, (bit_len + 7U) / 8U);
    }

    for (uint32_t bit_pos = 0; bit_pos < bit_len; bit_pos++) {
        uint32_t tx_byte_idx = bit_pos / 8U;
        uint32_t tx_bit_idx = bit_pos % 8U;
        bool tdi = default_tdi_high;
        bool tms = end_after_shift && (bit_pos == (bit_len - 1U));

        if (tx_data != NULL) {
            if (!lsb_first) {
                tx_bit_idx = 7U - tx_bit_idx;
            }
            tdi = ((tx_data[tx_byte_idx] >> tx_bit_idx) & 0x01U) != 0U;
        }

        Jtag_Tick(jtag, tms, tdi);

        if (rx_data != NULL) {
            uint32_t rx_byte_idx = bit_pos / 8U;
            uint32_t rx_bit_idx = bit_pos % 8U;
            if (JTAG_TDO_IS_HIGH()) {
                rx_data[rx_byte_idx] |= (uint8_t)(1U << rx_bit_idx);
            }
        }
    }

    if (end_after_shift) {
        jtag->current_state = (jtag->current_state == JTAG_SDR) ? JTAG_E1D : JTAG_E1I;
    }
}

static void Jtag_ShiftCfgInFast(JtagContext *jtag, const uint8_t *tx_data, uint32_t byte_len,
                                bool end_after_shift)
{
    if ((jtag == NULL) || (tx_data == NULL) || (byte_len == 0U)) {
        return;
    }

    JTAG_GPIO_CLR(JTAG_GPIO_PORT, JTAG_TMS_PIN);

    for (uint32_t byte_idx = 0; byte_idx < byte_len; byte_idx++) {
        uint8_t value = tx_data[byte_idx];
        bool is_last_byte = (byte_idx == (byte_len - 1U));

        if (is_last_byte && end_after_shift) {
            for (uint8_t mask = 0x80U; mask > 0x01U; mask >>= 1U) {
                JTAG_GPIO_WRITE(JTAG_GPIO_PORT, JTAG_TDI_PIN, (value & mask) != 0U);
                JTAG_GPIO_SET(JTAG_GPIO_PORT, JTAG_TCK_PIN);
                JTAG_GPIO_CLR(JTAG_GPIO_PORT, JTAG_TCK_PIN);
            }

            JTAG_GPIO_SET(JTAG_GPIO_PORT, JTAG_TMS_PIN);
            JTAG_GPIO_WRITE(JTAG_GPIO_PORT, JTAG_TDI_PIN, (value & 0x01U) != 0U);
            JTAG_GPIO_SET(JTAG_GPIO_PORT, JTAG_TCK_PIN);
            JTAG_GPIO_CLR(JTAG_GPIO_PORT, JTAG_TCK_PIN);
        } else {
            for (uint8_t mask = 0x80U; mask != 0U; mask >>= 1U) {
                JTAG_GPIO_WRITE(JTAG_GPIO_PORT, JTAG_TDI_PIN, (value & mask) != 0U);
                JTAG_GPIO_SET(JTAG_GPIO_PORT, JTAG_TCK_PIN);
                JTAG_GPIO_CLR(JTAG_GPIO_PORT, JTAG_TCK_PIN);
            }
        }
    }

    if (end_after_shift) {
        jtag->current_state = JTAG_E1D;
    }
}

static HAL_StatusTypeDef Jtag_WaitInitBHigh(JtagContext *jtag, uint32_t timeout_ms)
{
    g_fpga_state = FPGA_STATE_WAIT_INITB;

    while (HAL_GPIO_ReadPin(FPGA_INITB_PORT, FPGA_INITB_PIN) == GPIO_PIN_RESET) {
        Jtag_RunClocks(jtag, 32U, JTAG_RTI);
        osDelay(1);

        if (timeout_ms-- == 0U) {
            g_fpga_state = FPGA_STATE_FAILED;
            return HAL_TIMEOUT;
        }
    }

    return HAL_OK;
}

static bool Xsvf_ReadU8(const uint8_t **cursor, const uint8_t *end, uint8_t *value)
{
    if ((*cursor == NULL) || (value == NULL) || ((*cursor) >= end)) {
        return false;
    }

    *value = *(*cursor)++;
    return true;
}

static bool Xsvf_ReadU32BE(const uint8_t **cursor, const uint8_t *end, uint32_t *value)
{
    uint8_t b0, b1, b2, b3;

    if (!Xsvf_ReadU8(cursor, end, &b0) ||
        !Xsvf_ReadU8(cursor, end, &b1) ||
        !Xsvf_ReadU8(cursor, end, &b2) ||
        !Xsvf_ReadU8(cursor, end, &b3)) {
        return false;
    }

    *value = ((uint32_t)b0 << 24) | ((uint32_t)b1 << 16) | ((uint32_t)b2 << 8) | (uint32_t)b3;
    return true;
}

static bool Xsvf_ReadBytes(const uint8_t **cursor, const uint8_t *end, uint8_t *dst, uint32_t len)
{
    if (((uint32_t)(end - *cursor)) < len) {
        return false;
    }

    memcpy(dst, *cursor, len);
    *cursor += len;
    return true;
}

static JtagState Xsvf_MapState(uint8_t xsvf_state, bool ir_path)
{
    switch (xsvf_state) {
    case 0x00: return JTAG_TLR;
    case 0x01: return JTAG_RTI;
    case 0x02: return JTAG_SDS;
    case 0x03: return JTAG_CDR;
    case 0x04: return JTAG_SDR;
    case 0x05: return JTAG_E1D;
    case 0x06: return JTAG_PDR;
    case 0x07: return JTAG_E2D;
    case 0x08: return JTAG_UDR;
    case 0x09: return JTAG_SIS;
    case 0x0A: return JTAG_CIR;
    case 0x0B: return JTAG_SIR;
    case 0x0C: return JTAG_E1I;
    case 0x0D: return JTAG_PIR;
    case 0x0E: return JTAG_E2I;
    case 0x0F: return JTAG_UIR;
    default:   return ir_path ? JTAG_RTI : JTAG_RTI;
    }
}

static HAL_StatusTypeDef Xsvf_RunTest(JtagContext *jtag, uint32_t clocks_and_us)
{
    Jtag_RunClocks(jtag, clocks_and_us, JTAG_RTI);

    if (clocks_and_us >= 1000U) {
        osDelay(clocks_and_us / 1000U);
        clocks_and_us %= 1000U;
    }

    if (clocks_and_us != 0U) {
        FPGA_Delay_NS(clocks_and_us * 1000U);
    }

    return HAL_OK;
}

static bool Xsvf_CompareMasked(const uint8_t *actual, const uint8_t *expected,
                               const uint8_t *mask, uint32_t len, bool use_mask)
{
    for (uint32_t i = 0; i < len; i++) {
        uint8_t compare_mask = use_mask ? mask[i] : 0xFFU;
        if (((actual[i] ^ expected[i]) & compare_mask) != 0U) {
            return false;
        }
    }

    return true;
}

static bool Jtag_ShiftXsvf(JtagContext *jtag, const uint8_t *tx_data, uint8_t *rx_data,
                           uint32_t bit_len, bool is_ir, JtagState end_state)
{
    uint32_t byte_len = (bit_len + 7U) / 8U;
    bool exit_after = is_ir ? (end_state != JTAG_SIR) : (end_state != JTAG_SDR);

    if ((bit_len == 0U) || (jtag == NULL)) {
        return false;
    }

    if (is_ir) {
        Jtag_GotoState(jtag, JTAG_SIR);
    } else {
        Jtag_GotoState(jtag, JTAG_SDR);
    }

    if (rx_data != NULL) {
        memset(rx_data, 0, byte_len);
    }

    for (uint32_t bit_pos = 0; bit_pos < bit_len; bit_pos++) {
        uint32_t logical_byte = bit_pos / 8U;
        uint32_t logical_bit = bit_pos % 8U;
        uint32_t src_byte = (byte_len - 1U) - logical_byte;
        bool tdi = false;
        bool tms = exit_after && (bit_pos == (bit_len - 1U));

        if (tx_data != NULL) {
            tdi = ((tx_data[src_byte] >> logical_bit) & 0x01U) != 0U;
        }

        Jtag_Tick(jtag, tms, tdi);

        if (rx_data != NULL && JTAG_TDO_IS_HIGH()) {
            rx_data[src_byte] |= (uint8_t)(1U << logical_bit);
        }
    }

    if (exit_after) {
        jtag->current_state = is_ir ? JTAG_E1I : JTAG_E1D;
    }

    Jtag_GotoState(jtag, end_state);
    return true;
}

void Jtag_WriteInstruction(JtagContext *jtag, uint8_t inst, JtagState end_state)
{
    uint8_t inst_buf[1] = {inst};
    bool exit_after = (end_state != JTAG_SIR);

    Jtag_GotoState(jtag, JTAG_SIR);
    Jtag_ShiftRawOrder(jtag, inst_buf, NULL, XILINX_IR_LEN, exit_after, true, true);
    Jtag_GotoState(jtag, end_state);
}

void Jtag_WriteData(JtagContext *jtag, const uint8_t *data, uint32_t bit_len, JtagState end_state)
{
    bool exit_after = (end_state != JTAG_SDR);

    Jtag_GotoState(jtag, JTAG_SDR);
    Jtag_ShiftRawOrder(jtag, data, NULL, bit_len, exit_after, true, false);

    if (end_state != JTAG_SDR) {
        Jtag_GotoState(jtag, end_state);
    }
}

void Jtag_ReadData(JtagContext *jtag, uint8_t *data, uint32_t bit_len, JtagState end_state)
{
    bool exit_after = (end_state != JTAG_SDR);

    Jtag_GotoState(jtag, JTAG_SDR);
    Jtag_ShiftRawOrder(jtag, NULL, data, bit_len, exit_after, true, true);
    Jtag_GotoState(jtag, end_state);
}

HAL_StatusTypeDef Jtag_ConfigureFromSdram(uint32_t bin_size)
{
    if ((bin_size == 0U) || (bin_size > SDRAM_TOTAL_SIZE)) {
        CDC_Transmit_FS((uint8_t *)"[JTAG] Invalid bin size\r\n", 25);
        return HAL_ERROR;
    }

    send_total = 0U;

    JtagTransfer xfer = {0};
    JtagContext jtag;
    JtagHalCallbacks *hal = BSP_Jtag_GetHalOps();
    uint8_t *p_sdram = (uint8_t *)SDRAM_BASE_ADDR;
    uint32_t batch_size = JTAG_BUFFER_SIZE;

    Jtag_Init(&jtag, hal, &xfer);

    CDC_Transmit_FS((uint8_t *)"[JTAG] JPROGRAM\r\n", 17);
    Jtag_WriteInstruction(&jtag, XILINX_INST_JPROGRAM, JTAG_RTI);
    Jtag_Reset(&jtag);
    Jtag_GotoState(&jtag, JTAG_RTI);
    osDelay(10);

    CDC_Transmit_FS((uint8_t *)"[JTAG] BYPASS wait INIT_B\r\n", 27);
    Jtag_WriteInstruction(&jtag, XILINX_INST_BYPASS, JTAG_RTI);
    if (Jtag_WaitInitBHigh(&jtag, 10000U) != HAL_OK) {
        CDC_Transmit_FS((uint8_t *)"[JTAG] INIT_B timeout after JPROGRAM\r\n", 38);
				osDelay(10);
        return HAL_TIMEOUT;
    }

    if (HAL_GPIO_ReadPin(FPGA_INITB_PORT, FPGA_INITB_PIN) != GPIO_PIN_SET) {
        CDC_Transmit_FS((uint8_t *)"[JTAG] INIT_B abnormal\r\n", 24);
				osDelay(10);
        g_fpga_state = FPGA_STATE_FAILED;
        return HAL_ERROR;
    }

    CDC_Transmit_FS((uint8_t *)"[JTAG] CFG_IN shift start\r\n", 27);
		osDelay(10);
    g_fpga_state = FPGA_STATE_SENDING;
    Jtag_WriteInstruction(&jtag, XILINX_INST_CFG_IN, JTAG_RTI);
    Jtag_GotoState(&jtag, JTAG_SDR);

    while (send_total < bin_size) {
        uint32_t current_batch = ((bin_size - send_total) > batch_size) ? batch_size : (bin_size - send_total);
        bool is_last_batch = ((send_total + current_batch) == bin_size);

        if (HAL_GPIO_ReadPin(FPGA_INITB_PORT, FPGA_INITB_PIN) == GPIO_PIN_RESET) {
            CDC_Transmit_FS((uint8_t *)"[JTAG] INIT_B dropped during CFG_IN\r\n", 36);
						osDelay(10);
            g_fpga_state = FPGA_STATE_FAILED;
            return HAL_ERROR;
        }

        Jtag_ShiftCfgInFast(&jtag, &p_sdram[send_total], current_batch, is_last_batch);
        send_total += current_batch;
    }

    Jtag_GotoState(&jtag, JTAG_RTI);

    CDC_Transmit_FS((uint8_t *)"[JTAG] JSTART\r\n", 15);
		osDelay(10);
    Jtag_WriteInstruction(&jtag, XILINX_INST_JSTART, JTAG_RTI);
    Jtag_RunClocks(&jtag, 2000U, JTAG_TLR);

    if (FPGA_Wait_DONE_High() != HAL_OK) {
        CDC_Transmit_FS((uint8_t *)"[JTAG] DONE timeout\r\n", 22);
				osDelay(10);
        return HAL_TIMEOUT;
    }

    g_fpga_state = FPGA_STATE_SUCCESS;
    CDC_Transmit_FS((uint8_t *)"[JTAG] Config complete\r\n", 24);
    return HAL_OK;
}

HAL_StatusTypeDef Xsvf_ExecuteFromSdram(uint32_t file_size)
{
    enum {
        XCOMPLETE = 0x00,
        XTDOMASK = 0x01,
        XSIR = 0x02,
        XSDR = 0x03,
        XRUNTEST = 0x04,
        XREPEAT = 0x07,
        XSDRSIZE = 0x08,
        XSDRTDO = 0x09,
        XSDRB = 0x0C,
        XSDRC = 0x0D,
        XSDRE = 0x0E,
        XSDRTDOB = 0x0F,
        XSDRTDOC = 0x10,
        XSDRTDOE = 0x11,
        XSTATE = 0x12,
        XENDIR = 0x13,
        XENDDR = 0x14,
        XSIR2 = 0x15,
        XCOMMENT = 0x16,
        XWAIT = 0x17
    };

    static uint8_t tdi_buf[JTAG_BUFFER_SIZE];
    static uint8_t tdo_expected[JTAG_BUFFER_SIZE];
    static uint8_t tdo_mask[JTAG_BUFFER_SIZE];
    static uint8_t rx_buf[JTAG_BUFFER_SIZE];

    if ((file_size == 0U) || (file_size > SDRAM_TOTAL_SIZE)) {
        CDC_Transmit_FS((uint8_t *)"[XSVF] Invalid file size\r\n", 27);
        return HAL_ERROR;
    }

    const uint8_t *cursor = (const uint8_t *)SDRAM_BASE_ADDR;
    const uint8_t *end = cursor + file_size;
    uint32_t sdr_bit_len = 0U;
    uint32_t sdr_byte_len = 0U;
    uint32_t xruntest = 0U;
    uint8_t xrepeat = 0U;
    bool tdomask_valid = false;
    bool tdo_expected_valid = false;
    JtagState endir_state = JTAG_RTI;
    JtagState enddr_state = JTAG_RTI;
    JtagTransfer xfer = {0};
    JtagContext jtag;
    JtagHalCallbacks *hal = BSP_Jtag_GetHalOps();

    memset(tdo_mask, 0xFF, sizeof(tdo_mask));
    Jtag_Init(&jtag, hal, &xfer);
    g_fpga_state = FPGA_STATE_SENDING;

    while (cursor < end) {
        uint8_t cmd = 0U;

        if (!Xsvf_ReadU8(&cursor, end, &cmd)) {
            g_fpga_state = FPGA_STATE_FAILED;
            return HAL_ERROR;
        }

        if (cmd == XCOMPLETE) {
            g_fpga_state = FPGA_STATE_SUCCESS;
            CDC_Transmit_FS((uint8_t *)"[XSVF] Execute complete\r\n", 25);
            return HAL_OK;
        }

        switch (cmd) {
        case XTDOMASK:
            if ((sdr_byte_len == 0U) || (sdr_byte_len > sizeof(tdo_mask)) ||
                !Xsvf_ReadBytes(&cursor, end, tdo_mask, sdr_byte_len)) {
                g_fpga_state = FPGA_STATE_FAILED;
                return HAL_ERROR;
            }
            tdomask_valid = true;
            break;

        case XSDRSIZE:
            if (!Xsvf_ReadU32BE(&cursor, end, &sdr_bit_len)) {
                g_fpga_state = FPGA_STATE_FAILED;
                return HAL_ERROR;
            }
            sdr_byte_len = (sdr_bit_len + 7U) / 8U;
            if ((sdr_byte_len == 0U) || (sdr_byte_len > sizeof(tdi_buf))) {
                CDC_Transmit_FS((uint8_t *)"[XSVF] XSDRSIZE too large\r\n", 29);
                g_fpga_state = FPGA_STATE_FAILED;
                return HAL_ERROR;
            }
            break;

        case XRUNTEST:
            if (!Xsvf_ReadU32BE(&cursor, end, &xruntest)) {
                g_fpga_state = FPGA_STATE_FAILED;
                return HAL_ERROR;
            }
            break;

        case XREPEAT:
            if (!Xsvf_ReadU8(&cursor, end, &xrepeat)) {
                g_fpga_state = FPGA_STATE_FAILED;
                return HAL_ERROR;
            }
            break;

        case XENDIR:
        {
            uint8_t state = 0U;
            if (!Xsvf_ReadU8(&cursor, end, &state)) {
                g_fpga_state = FPGA_STATE_FAILED;
                return HAL_ERROR;
            }
            endir_state = (state == 0x01U) ? JTAG_PIR : JTAG_RTI;
            break;
        }

        case XENDDR:
        {
            uint8_t state = 0U;
            if (!Xsvf_ReadU8(&cursor, end, &state)) {
                g_fpga_state = FPGA_STATE_FAILED;
                return HAL_ERROR;
            }
            enddr_state = (state == 0x01U) ? JTAG_PDR : JTAG_RTI;
            break;
        }

        case XSTATE:
        {
            uint8_t state = 0U;
            if (!Xsvf_ReadU8(&cursor, end, &state)) {
                g_fpga_state = FPGA_STATE_FAILED;
                return HAL_ERROR;
            }
            if (state == 0x00U) {
                Jtag_Reset(&jtag);
            } else {
                Jtag_GotoState(&jtag, Xsvf_MapState(state, false));
            }
            break;
        }

        case XWAIT:
        {
            uint8_t wait_state = 0U;
            uint8_t end_state = 0U;
            uint32_t wait_time = 0U;
            if (!Xsvf_ReadU8(&cursor, end, &wait_state) ||
                !Xsvf_ReadU8(&cursor, end, &end_state) ||
                !Xsvf_ReadU32BE(&cursor, end, &wait_time)) {
                g_fpga_state = FPGA_STATE_FAILED;
                return HAL_ERROR;
            }
            Jtag_GotoState(&jtag, Xsvf_MapState(wait_state, false));
            if (wait_time >= 1000U) {
                osDelay(wait_time / 1000U);
                wait_time %= 1000U;
            }
            if (wait_time != 0U) {
                FPGA_Delay_NS(wait_time * 1000U);
            }
            Jtag_GotoState(&jtag, Xsvf_MapState(end_state, false));
            break;
        }

        case XCOMMENT:
            while ((cursor < end) && (*cursor != 0U)) {
                cursor++;
            }
            if (cursor < end) {
                cursor++;
            }
            break;

        case XSIR:
        {
            uint8_t ir_len = 0U;
            uint32_t ir_byte_len = 0U;
            if (!Xsvf_ReadU8(&cursor, end, &ir_len)) {
                g_fpga_state = FPGA_STATE_FAILED;
                return HAL_ERROR;
            }
            ir_byte_len = ((uint32_t)ir_len + 7U) / 8U;
            if ((ir_byte_len == 0U) || (ir_byte_len > sizeof(tdi_buf)) ||
                !Xsvf_ReadBytes(&cursor, end, tdi_buf, ir_byte_len) ||
                !Jtag_ShiftXsvf(&jtag, tdi_buf, NULL, ir_len, true,
                                (xruntest != 0U) ? JTAG_RTI : endir_state)) {
                g_fpga_state = FPGA_STATE_FAILED;
                return HAL_ERROR;
            }
            if (xruntest != 0U) {
                Xsvf_RunTest(&jtag, xruntest);
            }
            break;
        }

        case XSIR2:
        {
            uint8_t len_hi = 0U;
            uint8_t len_lo = 0U;
            uint32_t ir_len = 0U;
            uint32_t ir_byte_len = 0U;
            if (!Xsvf_ReadU8(&cursor, end, &len_hi) ||
                !Xsvf_ReadU8(&cursor, end, &len_lo)) {
                g_fpga_state = FPGA_STATE_FAILED;
                return HAL_ERROR;
            }
            ir_len = ((uint32_t)len_hi << 8) | (uint32_t)len_lo;
            ir_byte_len = (ir_len + 7U) / 8U;
            if ((ir_byte_len == 0U) || (ir_byte_len > sizeof(tdi_buf)) ||
                !Xsvf_ReadBytes(&cursor, end, tdi_buf, ir_byte_len) ||
                !Jtag_ShiftXsvf(&jtag, tdi_buf, NULL, ir_len, true,
                                (xruntest != 0U) ? JTAG_RTI : endir_state)) {
                g_fpga_state = FPGA_STATE_FAILED;
                return HAL_ERROR;
            }
            if (xruntest != 0U) {
                Xsvf_RunTest(&jtag, xruntest);
            }
            break;
        }

        case XSDR:
        case XSDRTDO:
        case XSDRB:
        case XSDRC:
        case XSDRE:
        case XSDRTDOB:
        case XSDRTDOC:
        case XSDRTDOE:
        {
            uint32_t attempts = (uint32_t)xrepeat + 1U;
            JtagState end_state = enddr_state;
            bool compare = false;
            bool use_mask = false;

            if ((sdr_byte_len == 0U) || (sdr_byte_len > sizeof(tdi_buf)) ||
                !Xsvf_ReadBytes(&cursor, end, tdi_buf, sdr_byte_len)) {
                g_fpga_state = FPGA_STATE_FAILED;
                return HAL_ERROR;
            }

            if ((cmd == XSDRTDO) || (cmd == XSDRTDOB) || (cmd == XSDRTDOC) || (cmd == XSDRTDOE)) {
                if (!Xsvf_ReadBytes(&cursor, end, tdo_expected, sdr_byte_len)) {
                    g_fpga_state = FPGA_STATE_FAILED;
                    return HAL_ERROR;
                }
                tdo_expected_valid = true;
            }

            if ((cmd == XSDR) && tdo_expected_valid) {
                compare = true;
                use_mask = tdomask_valid;
            } else if ((cmd == XSDRTDO) || (cmd == XSDRTDOB) || (cmd == XSDRTDOC) || (cmd == XSDRTDOE)) {
                compare = true;
                use_mask = (cmd == XSDRTDO);
            }

            if ((cmd == XSDRB) || (cmd == XSDRC) || (cmd == XSDRTDOB) || (cmd == XSDRTDOC)) {
                end_state = JTAG_SDR;
            } else if (xruntest != 0U) {
                end_state = JTAG_RTI;
            }

            do {
                if (!Jtag_ShiftXsvf(&jtag, tdi_buf, compare ? rx_buf : NULL, sdr_bit_len, false, end_state)) {
                    g_fpga_state = FPGA_STATE_FAILED;
                    return HAL_ERROR;
                }

                if (!compare || Xsvf_CompareMasked(rx_buf, tdo_expected, tdo_mask, sdr_byte_len, use_mask)) {
                    break;
                }
            } while (--attempts != 0U);

            if (compare && (attempts == 0U)) {
                CDC_Transmit_FS((uint8_t *)"[XSVF] TDO compare failed\r\n", 27);
                g_fpga_state = FPGA_STATE_FAILED;
                return HAL_ERROR;
            }

            if ((xruntest != 0U) &&
                ((cmd == XSDR) || (cmd == XSDRTDO) || (cmd == XSDRE) || (cmd == XSDRTDOE))) {
                Xsvf_RunTest(&jtag, xruntest);
            }
            break;
        }

        default:
        {
            char msg[48] = {0};
            snprintf(msg, sizeof(msg), "[XSVF] Unsupported cmd 0x%02X\r\n", cmd);
            CDC_Transmit_FS((uint8_t *)msg, strlen(msg));
            g_fpga_state = FPGA_STATE_FAILED;
            return HAL_ERROR;
        }
        }
    }

    g_fpga_state = FPGA_STATE_FAILED;
    CDC_Transmit_FS((uint8_t *)"[XSVF] Missing XCOMPLETE\r\n", 25);
    return HAL_ERROR;
}

JtagHalCallbacks *BSP_Jtag_GetHalOps(void)
{
    jtag_hal_ops.SetTMS = BSP_Jtag_SetTms;
    jtag_hal_ops.SetTCK = BSP_Jtag_SetTck;
    jtag_hal_ops.SetTDI = BSP_Jtag_SetTdi;
    jtag_hal_ops.ReadTDO = BSP_Jtag_ReadTdo;
    return &jtag_hal_ops;
}

void FPGA_Switch_Mode(FPGA_ModeType mode)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOE_CLK_ENABLE();

    // 先把所有复用引脚全部去初始化
    HAL_GPIO_DeInit(GPIOE, GPIO_PIN_2 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6);

    if (mode == FPGA_MODE_JTAG)
    {
        // JTAG 模式
        // TCK(PE2) TDI(PE6) TMS(PE4) 推挽输出
        GPIO_InitStruct.Pin = JTAG_TCK_PIN | JTAG_TDI_PIN | JTAG_TMS_PIN;
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        HAL_GPIO_Init(JTAG_GPIO_PORT, &GPIO_InitStruct);

        // TDO(PE5) 上拉输入
        GPIO_InitStruct.Pin = JTAG_TDO_PIN;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        HAL_GPIO_Init(JTAG_GPIO_PORT, &GPIO_InitStruct);
    }
    else
    {
        // Slave Serial 模式
        // CCLK(PE2) DATA0(PE6) 由 SPI 控制 => 复用推挽
        MX_SPI4_Init();
    }
}
