#include "fpga_config.h"
#include "spi_bridge_bin.h"

FPGA_StateTypeDef g_fpga_state = FPGA_STATE_IDLE;
JtagHalCallbacks jtag_hal_ops;
uint8_t g_fpga_config_start = 0;
static JtagTransfer g_jtag_xfer = {0};

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

        JTAG_GPIO_WRITE(JTAG_GPIO_PORT, JTAG_TMS_PIN, tms);
        JTAG_GPIO_WRITE(JTAG_GPIO_PORT, JTAG_TDI_PIN, tdi);

        if (JTAG_EDGE_DELAY_NS != 0U) {
            FPGA_Delay_NS(JTAG_EDGE_DELAY_NS);
        }

        JTAG_GPIO_SET(JTAG_GPIO_PORT, JTAG_TCK_PIN);

        if (JTAG_EDGE_DELAY_NS != 0U) {
            FPGA_Delay_NS(JTAG_EDGE_DELAY_NS);
        }

        if (rx_data != NULL) {
            uint32_t rx_byte_idx = bit_pos / 8U;
            uint32_t rx_bit_idx = bit_pos % 8U;
            if (JTAG_TDO_IS_HIGH()) {
                rx_data[rx_byte_idx] |= (uint8_t)(1U << rx_bit_idx);
            }
        }

        JTAG_GPIO_CLR(JTAG_GPIO_PORT, JTAG_TCK_PIN);
    }

    if (end_after_shift) {
        jtag->current_state = (jtag->current_state == JTAG_SDR) ? JTAG_E1D : JTAG_E1I;
    }
}

static void Jtag_ShiftCfgInFast(JtagContext *jtag, const uint8_t *tx_data, uint32_t byte_len,
                                bool end_after_shift);
static HAL_StatusTypeDef Jtag_WaitInitBHigh(JtagContext *jtag, uint32_t timeout_ms);
static HAL_StatusTypeDef Jtag_ConfigureFromBuffer(const uint8_t *bin_data, uint32_t bin_size);
static HAL_StatusTypeDef Bridge_ShiftFrame(JtagContext *jtag, const uint8_t *tx_frame, uint8_t *rx_frame);
static HAL_StatusTypeDef Bridge_CommandFrame(JtagContext *jtag, uint8_t opcode, uint32_t arg,
                                             uint8_t *rx_frame_out, uint32_t timeout_cycles);
static HAL_StatusTypeDef Bridge_Command(JtagContext *jtag, uint8_t opcode, uint32_t arg,
                                        uint8_t *rx_data0, uint32_t timeout_cycles);
static void Bridge_LogFrame(const char *tag, uint8_t opcode, const uint8_t *frame);
static HAL_StatusTypeDef Bridge_SetCs(JtagContext *jtag, bool cs_high);
static HAL_StatusTypeDef Bridge_SetDivider(JtagContext *jtag, uint16_t divider);
static HAL_StatusTypeDef Bridge_Xfer8(JtagContext *jtag, uint8_t tx_data, uint8_t *rx_data);
static HAL_StatusTypeDef Bridge_ReadStatusEx(JtagContext *jtag, uint8_t flags[4]);
static HAL_StatusTypeDef spi_flash_read_status(JtagContext *jtag, uint8_t *status);
static HAL_StatusTypeDef spi_flash_read_flag_status(JtagContext *jtag, uint8_t *status);
static HAL_StatusTypeDef spi_flash_wait_ready(JtagContext *jtag, uint32_t timeout_ms);
static HAL_StatusTypeDef spi_flash_read_jedec_id(JtagContext *jtag, uint8_t jedec_id[3]);
static HAL_StatusTypeDef spi_flash_read_buffer(JtagContext *jtag, uint32_t addr, uint8_t *data, uint16_t len);
static HAL_StatusTypeDef spi_flash_write_enable(JtagContext *jtag);
static HAL_StatusTypeDef spi_flash_reset_memory(JtagContext *jtag);

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

static HAL_StatusTypeDef Jtag_ConfigureFromBuffer(const uint8_t *bin_data, uint32_t bin_size)
{
    if ((bin_data == NULL) || (bin_size == 0U)) {
        CDC_Transmit_FS((uint8_t *)"[JTAG] Invalid bitstream\r\n", 26);
        return HAL_ERROR;
    }

    send_total = 0U;

    JtagContext jtag;
    JtagHalCallbacks *hal = BSP_Jtag_GetHalOps();
    uint32_t batch_size = JTAG_BUFFER_SIZE;

    memset(&g_jtag_xfer, 0, sizeof(g_jtag_xfer));
    Jtag_Init(&jtag, hal, &g_jtag_xfer);

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

        Jtag_ShiftCfgInFast(&jtag, &bin_data[send_total], current_batch, is_last_batch);
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

HAL_StatusTypeDef Jtag_ConfigureFromSdram(uint32_t bin_size)
{
    if ((bin_size == 0U) || (bin_size > SDRAM_TOTAL_SIZE)) {
        CDC_Transmit_FS((uint8_t *)"[JTAG] Invalid bin size\r\n", 25);
        return HAL_ERROR;
    }

    return Jtag_ConfigureFromBuffer((const uint8_t *)SDRAM_BASE_ADDR, bin_size);
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

    /* PE2/PE6 are shared by SPI4 and bit-banged JTAG, so force SPI deinit
       before changing the pin mux to guarantee HAL re-runs MSP init later. */
    (void)HAL_SPI_DeInit(&hspi4);
    HAL_GPIO_DeInit(GPIOE, GPIO_PIN_2 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6);

    if (mode == FPGA_MODE_JTAG)
    {
        GPIO_InitStruct.Pin = JTAG_TCK_PIN | JTAG_TDI_PIN | JTAG_TMS_PIN;
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        HAL_GPIO_Init(JTAG_GPIO_PORT, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = JTAG_TDO_PIN;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        HAL_GPIO_Init(JTAG_GPIO_PORT, &GPIO_InitStruct);
    }
    else
    {
        MX_SPI4_Init();
    }
}

/**
 * @brief  AXI閸愭瑦鎼锋担婊愮窗閸氭垶瀵氱€规艾婀撮崸鈧崘娆忓弳32娴ｅ秵鏆熼幑?
 * @param  jtag JTAG娑撳﹣绗呴弬鍥у綖閺?
 * @param  addr 32娴ｅ伯XI閸︽澘娼?
 * @param  data 瀵板懎鍟撻崗銉ф畱32娴ｅ秵鏆熼幑?
 */
static HAL_StatusTypeDef Bridge_ShiftFrame(JtagContext *jtag, const uint8_t *tx_frame, uint8_t *rx_frame)
{
    if ((jtag == NULL) || (tx_frame == NULL) || (rx_frame == NULL)) {
        return HAL_ERROR;
    }

    memset(rx_frame, 0, BRIDGE_FRAME_BYTES);
    Jtag_GotoState(jtag, JTAG_SDR);
    Jtag_ShiftRawOrder(jtag, tx_frame, rx_frame, BRIDGE_FRAME_BITS, true, true, false);
    Jtag_GotoState(jtag, JTAG_RTI);
    return HAL_OK;
}

static void Bridge_LogFrame(const char *tag, uint8_t opcode, const uint8_t *frame)
{
    char log_buf[96] = {0};
    uint16_t log_len = 0U;

    if ((tag == NULL) || (frame == NULL)) {
        return;
    }

    log_len = (uint16_t)snprintf(log_buf, sizeof(log_buf),
                                 "[BRIDGE] %s op=%02X rx=%02X %02X %02X %02X %02X\r\n",
                                 tag, opcode,
                                 frame[0], frame[1], frame[2], frame[3], frame[4]);
    if (log_len > 0U) {
        CDC_Transmit_FS((uint8_t *)log_buf, log_len);
    }
}

static HAL_StatusTypeDef Bridge_CommandFrame(JtagContext *jtag, uint8_t opcode, uint32_t arg,
                                             uint8_t *rx_frame_out, uint32_t timeout_cycles)
{
    uint8_t tx_frame[BRIDGE_FRAME_BYTES] = {0};
    uint8_t rx_frame[BRIDGE_FRAME_BYTES] = {0};
    uint8_t nop_frame[BRIDGE_FRAME_BYTES] = {0};

    tx_frame[0] = opcode;
    tx_frame[1] = (uint8_t)(arg & 0xFFU);
    tx_frame[2] = (uint8_t)((arg >> 8) & 0xFFU);
    tx_frame[3] = (uint8_t)((arg >> 16) & 0xFFU);
    tx_frame[4] = (uint8_t)((arg >> 24) & 0xFFU);

    if (Bridge_ShiftFrame(jtag, tx_frame, rx_frame) != HAL_OK) {
        Bridge_LogFrame("shift-fail", opcode, rx_frame);
        return HAL_ERROR;
    }

    /* The USER1 bridge is one frame deep, so the first NOP after UPDATE
       usually returns the previous response. Prime the pipeline first. */
    if (Bridge_ShiftFrame(jtag, nop_frame, rx_frame) != HAL_OK) {
        Bridge_LogFrame("prime-fail", opcode, rx_frame);
        return HAL_ERROR;
    }

    while (timeout_cycles-- > 0U) {
        if (Bridge_ShiftFrame(jtag, nop_frame, rx_frame) != HAL_OK) {
            Bridge_LogFrame("poll-fail", opcode, rx_frame);
            return HAL_ERROR;
        }

        if (rx_frame[4] == BRIDGE_STATUS_BUSY) {
            continue;
        }

        if (rx_frame[4] != BRIDGE_STATUS_OK) {
            Bridge_LogFrame("bad-status", opcode, rx_frame);
            return HAL_ERROR;
        }

        if (rx_frame_out != NULL) {
            memcpy(rx_frame_out, rx_frame, BRIDGE_FRAME_BYTES);
        }
        return HAL_OK;
    }

    Bridge_LogFrame("timeout", opcode, rx_frame);
    return HAL_TIMEOUT;
}

static HAL_StatusTypeDef Bridge_Command(JtagContext *jtag, uint8_t opcode, uint32_t arg,
                                        uint8_t *rx_data0, uint32_t timeout_cycles)
{
    uint8_t rx_frame[BRIDGE_FRAME_BYTES] = {0};

    if (Bridge_CommandFrame(jtag, opcode, arg, rx_frame, timeout_cycles) != HAL_OK) {
        return HAL_ERROR;
    }

    if (rx_data0 != NULL) {
        *rx_data0 = rx_frame[0];
    }

    return HAL_OK;
}

static HAL_StatusTypeDef Bridge_SetCs(JtagContext *jtag, bool cs_high)
{
    return Bridge_Command(jtag, BRIDGE_CMD_SET_CS, cs_high ? 1U : 0U, NULL, 16U);
}

static HAL_StatusTypeDef Bridge_SetDivider(JtagContext *jtag, uint16_t divider)
{
    return Bridge_Command(jtag, BRIDGE_CMD_SET_DIV, divider, NULL, 16U);
}

static HAL_StatusTypeDef Bridge_Xfer8(JtagContext *jtag, uint8_t tx_data, uint8_t *rx_data)
{
    return Bridge_Command(jtag, BRIDGE_CMD_XFER8, tx_data, rx_data, 256U);
}

static HAL_StatusTypeDef Bridge_ReadStatusEx(JtagContext *jtag, uint8_t flags[4])
{
    uint8_t rx_frame[BRIDGE_FRAME_BYTES] = {0};

    if ((jtag == NULL) || (flags == NULL)) {
        return HAL_ERROR;
    }

    if (Bridge_CommandFrame(jtag, BRIDGE_CMD_STATUS_EX, 0U, rx_frame, 32U) != HAL_OK) {
        return HAL_ERROR;
    }

    memcpy(flags, rx_frame, 4U);
    return HAL_OK;
}

static HAL_StatusTypeDef Bridge_Echo(JtagContext *jtag, uint32_t arg, uint8_t echoed[4])
{
    uint8_t rx_frame[BRIDGE_FRAME_BYTES] = {0};

    if ((jtag == NULL) || (echoed == NULL)) {
        return HAL_ERROR;
    }

    if (Bridge_CommandFrame(jtag, BRIDGE_CMD_ECHO, arg, rx_frame, 32U) != HAL_OK) {
        return HAL_ERROR;
    }

    memcpy(echoed, rx_frame, 4U);
    return HAL_OK;
}

HAL_StatusTypeDef spi_flash_init(JtagContext *jtag)
{
    uint8_t nop_frame[BRIDGE_FRAME_BYTES] = {0};
    uint8_t rx_frame[BRIDGE_FRAME_BYTES] = {0};
    uint8_t echo_bytes[4] = {0};
    char log_buf[96] = {0};
    uint16_t log_len = 0U;

    if (jtag == NULL) {
        return HAL_ERROR;
    }

    Jtag_WriteInstruction(jtag, XILINX_INST_USER1, JTAG_RTI);

    if (Bridge_ShiftFrame(jtag, nop_frame, rx_frame) != HAL_OK) {
        CDC_Transmit_FS((uint8_t*)"[BRIDGE] Initial NOP shift failed\r\n",
                        sizeof("[BRIDGE] Initial NOP shift failed\r\n") - 1U);
        return HAL_ERROR;
    }
    Bridge_LogFrame("probe", BRIDGE_CMD_NOP, rx_frame);

    if (Bridge_Echo(jtag, 0x12345678U, echo_bytes) != HAL_OK) {
        CDC_Transmit_FS((uint8_t*)"[BRIDGE] ECHO command failed\r\n",
                        sizeof("[BRIDGE] ECHO command failed\r\n") - 1U);
        return HAL_ERROR;
    }

    log_len = (uint16_t)snprintf(log_buf, sizeof(log_buf),
                                 "[BRIDGE] ECHO rx=%02X %02X %02X %02X expect=06 78 56 34\r\n",
                                 echo_bytes[0], echo_bytes[1], echo_bytes[2], echo_bytes[3]);
    if (log_len > 0U) {
        CDC_Transmit_FS((uint8_t*)log_buf, log_len);
    }

    if ((echo_bytes[0] != BRIDGE_CMD_ECHO) ||
        (echo_bytes[1] != 0x78U) ||
        (echo_bytes[2] != 0x56U) ||
        (echo_bytes[3] != 0x34U)) {
        CDC_Transmit_FS((uint8_t*)"[BRIDGE] ECHO mismatch\r\n",
                        sizeof("[BRIDGE] ECHO mismatch\r\n") - 1U);
        return HAL_ERROR;
    }

    if (Bridge_SetCs(jtag, true) != HAL_OK) {
        CDC_Transmit_FS((uint8_t*)"[BRIDGE] SET_CS failed\r\n",
                        sizeof("[BRIDGE] SET_CS failed\r\n") - 1U);
        return HAL_ERROR;
    }

    if (Bridge_SetDivider(jtag, BRIDGE_SPI_CLK_DIV) != HAL_OK) {
        CDC_Transmit_FS((uint8_t*)"[BRIDGE] SET_DIV failed\r\n",
                        sizeof("[BRIDGE] SET_DIV failed\r\n") - 1U);
        return HAL_ERROR;
    }

    return HAL_OK;
}

static HAL_StatusTypeDef spi_flash_read_status(JtagContext *jtag, uint8_t *status)
{
    uint8_t rx_data = 0U;

    if ((jtag == NULL) || (status == NULL)) {
        return HAL_ERROR;
    }

    if (Bridge_SetCs(jtag, false) != HAL_OK) {
        return HAL_ERROR;
    }
    if (Bridge_Xfer8(jtag, SPI_CMD_READ_STATUS1, NULL) != HAL_OK) {
        (void)Bridge_SetCs(jtag, true);
        return HAL_ERROR;
    }
    if (Bridge_Xfer8(jtag, 0xFFU, &rx_data) != HAL_OK) {
        (void)Bridge_SetCs(jtag, true);
        return HAL_ERROR;
    }
    if (Bridge_SetCs(jtag, true) != HAL_OK) {
        return HAL_ERROR;
    }

    *status = rx_data;
    return HAL_OK;
}

static HAL_StatusTypeDef spi_flash_read_flag_status(JtagContext *jtag, uint8_t *status)
{
    uint8_t rx_data = 0U;

    if ((jtag == NULL) || (status == NULL)) {
        return HAL_ERROR;
    }

    if (Bridge_SetCs(jtag, false) != HAL_OK) {
        return HAL_ERROR;
    }
    if (Bridge_Xfer8(jtag, SPI_CMD_READ_FLAG_STATUS, NULL) != HAL_OK) {
        (void)Bridge_SetCs(jtag, true);
        return HAL_ERROR;
    }
    if (Bridge_Xfer8(jtag, 0xFFU, &rx_data) != HAL_OK) {
        (void)Bridge_SetCs(jtag, true);
        return HAL_ERROR;
    }
    if (Bridge_SetCs(jtag, true) != HAL_OK) {
        return HAL_ERROR;
    }

    *status = rx_data;
    return HAL_OK;
}

static HAL_StatusTypeDef spi_flash_wait_ready(JtagContext *jtag, uint32_t timeout_ms)
{
    uint8_t status = 0U;

    while (timeout_ms-- > 0U) {
        if (spi_flash_read_status(jtag, &status) != HAL_OK) {
            return HAL_ERROR;
        }

        if ((status & 0x01U) == 0U) {
            return HAL_OK;
        }

        osDelay(1);
    }

    return HAL_TIMEOUT;
}

static HAL_StatusTypeDef spi_flash_write_enable(JtagContext *jtag)
{
    uint8_t status = 0U;

    if (Bridge_SetCs(jtag, false) != HAL_OK) {
        return HAL_ERROR;
    }
    if (Bridge_Xfer8(jtag, SPI_CMD_WRITE_ENABLE, NULL) != HAL_OK) {
        (void)Bridge_SetCs(jtag, true);
        return HAL_ERROR;
    }
    if (Bridge_SetCs(jtag, true) != HAL_OK) {
        return HAL_ERROR;
    }

    for (uint32_t retry = 0; retry < 50U; retry++) {
        if (spi_flash_read_status(jtag, &status) != HAL_OK) {
            return HAL_ERROR;
        }
        if ((status & 0x02U) != 0U) {
            return HAL_OK;
        }
        osDelay(1);
    }

    return HAL_TIMEOUT;
}

static HAL_StatusTypeDef spi_flash_reset_memory(JtagContext *jtag)
{
    if (jtag == NULL) {
        return HAL_ERROR;
    }

    if (Bridge_SetCs(jtag, false) != HAL_OK) {
        return HAL_ERROR;
    }
    if (Bridge_Xfer8(jtag, SPI_CMD_RESET_ENABLE, NULL) != HAL_OK) {
        (void)Bridge_SetCs(jtag, true);
        return HAL_ERROR;
    }
    if (Bridge_SetCs(jtag, true) != HAL_OK) {
        return HAL_ERROR;
    }

    if (Bridge_SetCs(jtag, false) != HAL_OK) {
        return HAL_ERROR;
    }
    if (Bridge_Xfer8(jtag, SPI_CMD_RESET_MEMORY, NULL) != HAL_OK) {
        (void)Bridge_SetCs(jtag, true);
        return HAL_ERROR;
    }
    if (Bridge_SetCs(jtag, true) != HAL_OK) {
        return HAL_ERROR;
    }

    osDelay(1);
    return HAL_OK;
}

static HAL_StatusTypeDef spi_flash_read_jedec_id(JtagContext *jtag, uint8_t jedec_id[3])
{
    uint8_t cmd_rx = 0U;
    char log_buf[96] = {0};
    uint16_t log_len = 0U;

    if ((jtag == NULL) || (jedec_id == NULL)) {
        return HAL_ERROR;
    }

    if (Bridge_SetCs(jtag, false) != HAL_OK) {
        return HAL_ERROR;
    }
    if (Bridge_Xfer8(jtag, SPI_CMD_READ_JEDEC_ID, &cmd_rx) != HAL_OK) {
        (void)Bridge_SetCs(jtag, true);
        return HAL_ERROR;
    }

    for (uint32_t i = 0; i < 3U; i++) {
        if (Bridge_Xfer8(jtag, 0xFFU, &jedec_id[i]) != HAL_OK) {
            (void)Bridge_SetCs(jtag, true);
            return HAL_ERROR;
        }
    }

    if (Bridge_SetCs(jtag, true) != HAL_OK) {
        return HAL_ERROR;
    }

    log_len = (uint16_t)snprintf(log_buf, sizeof(log_buf),
                                 "[FLASH] JEDEC raw cmd_rx=%02X id=%02X %02X %02X\r\n",
                                 cmd_rx, jedec_id[0], jedec_id[1], jedec_id[2]);
    if (log_len > 0U) {
        CDC_Transmit_FS((uint8_t *)log_buf, log_len);
    }

    return HAL_OK;
}

HAL_StatusTypeDef spi_flash_erase_sector(JtagContext *jtag, uint32_t sector_addr)
{
    if (spi_flash_write_enable(jtag) != HAL_OK) {
        return HAL_ERROR;
    }

    if (Bridge_SetCs(jtag, false) != HAL_OK) {
        return HAL_ERROR;
    }
    if (Bridge_Xfer8(jtag, SPI_CMD_SECTOR_ERASE, NULL) != HAL_OK) {
        (void)Bridge_SetCs(jtag, true);
        return HAL_ERROR;
    }
    if (Bridge_Xfer8(jtag, (uint8_t)((sector_addr >> 16) & 0xFFU), NULL) != HAL_OK) {
        (void)Bridge_SetCs(jtag, true);
        return HAL_ERROR;
    }
    if (Bridge_Xfer8(jtag, (uint8_t)((sector_addr >> 8) & 0xFFU), NULL) != HAL_OK) {
        (void)Bridge_SetCs(jtag, true);
        return HAL_ERROR;
    }
    if (Bridge_Xfer8(jtag, (uint8_t)(sector_addr & 0xFFU), NULL) != HAL_OK) {
        (void)Bridge_SetCs(jtag, true);
        return HAL_ERROR;
    }
    if (Bridge_SetCs(jtag, true) != HAL_OK) {
        return HAL_ERROR;
    }

    return spi_flash_wait_ready(jtag, 5000U);
}

HAL_StatusTypeDef spi_flash_program_page(JtagContext *jtag, uint32_t page_addr, const uint8_t *data, uint16_t len)
{
    if ((data == NULL) || (len == 0U) || (len > SPI_FLASH_PAGE_SIZE)) {
        return HAL_ERROR;
    }

    if (spi_flash_write_enable(jtag) != HAL_OK) {
        return HAL_ERROR;
    }

    if (Bridge_SetCs(jtag, false) != HAL_OK) {
        return HAL_ERROR;
    }
    if (Bridge_Xfer8(jtag, SPI_CMD_PAGE_PROGRAM, NULL) != HAL_OK) {
        (void)Bridge_SetCs(jtag, true);
        return HAL_ERROR;
    }
    if (Bridge_Xfer8(jtag, (uint8_t)((page_addr >> 16) & 0xFFU), NULL) != HAL_OK) {
        (void)Bridge_SetCs(jtag, true);
        return HAL_ERROR;
    }
    if (Bridge_Xfer8(jtag, (uint8_t)((page_addr >> 8) & 0xFFU), NULL) != HAL_OK) {
        (void)Bridge_SetCs(jtag, true);
        return HAL_ERROR;
    }
    if (Bridge_Xfer8(jtag, (uint8_t)(page_addr & 0xFFU), NULL) != HAL_OK) {
        (void)Bridge_SetCs(jtag, true);
        return HAL_ERROR;
    }

    for (uint16_t i = 0; i < len; i++) {
        if (Bridge_Xfer8(jtag, data[i], NULL) != HAL_OK) {
            (void)Bridge_SetCs(jtag, true);
            return HAL_ERROR;
        }
    }

    if (Bridge_SetCs(jtag, true) != HAL_OK) {
        return HAL_ERROR;
    }

    return spi_flash_wait_ready(jtag, 1000U);
}

static HAL_StatusTypeDef spi_flash_read_buffer(JtagContext *jtag, uint32_t addr, uint8_t *data, uint16_t len)
{
    if ((jtag == NULL) || (data == NULL) || (len == 0U)) {
        return HAL_ERROR;
    }

    if (Bridge_SetCs(jtag, false) != HAL_OK) {
        return HAL_ERROR;
    }
    if (Bridge_Xfer8(jtag, SPI_CMD_READ_DATA, NULL) != HAL_OK) {
        (void)Bridge_SetCs(jtag, true);
        return HAL_ERROR;
    }
    if (Bridge_Xfer8(jtag, (uint8_t)((addr >> 16) & 0xFFU), NULL) != HAL_OK) {
        (void)Bridge_SetCs(jtag, true);
        return HAL_ERROR;
    }
    if (Bridge_Xfer8(jtag, (uint8_t)((addr >> 8) & 0xFFU), NULL) != HAL_OK) {
        (void)Bridge_SetCs(jtag, true);
        return HAL_ERROR;
    }
    if (Bridge_Xfer8(jtag, (uint8_t)(addr & 0xFFU), NULL) != HAL_OK) {
        (void)Bridge_SetCs(jtag, true);
        return HAL_ERROR;
    }

    for (uint16_t i = 0; i < len; i++) {
        if (Bridge_Xfer8(jtag, 0xFFU, &data[i]) != HAL_OK) {
            (void)Bridge_SetCs(jtag, true);
            return HAL_ERROR;
        }
    }

    return Bridge_SetCs(jtag, true);
}

uint8_t spi_flash_read_byte(JtagContext *jtag, uint32_t addr)
{
    uint8_t rx_data = 0U;

    if (spi_flash_read_buffer(jtag, addr, &rx_data, 1U) != HAL_OK) {
        return 0xFFU;
    }

    return rx_data;
}

HAL_StatusTypeDef spi_flash_program_full(uint8_t *bin_data, uint32_t bin_len)
{
    JtagContext jtag;
    JtagHalCallbacks *hal = BSP_Jtag_GetHalOps();
    char log_buf[128] = {0};
    uint8_t bridge_status[4] = {0};
    uint8_t verify_buf[SPI_FLASH_PAGE_SIZE] = {0};
    uint8_t jedec_id[3] = {0};
    uint8_t status_reg = 0U;
    uint8_t flag_status_reg = 0U;
    uint16_t log_len = 0;

    if ((bin_data == NULL) || (bin_len == 0U) || (bin_len > SDRAM_TOTAL_SIZE)) {
        CDC_Transmit_FS((uint8_t*)"[ERROR] Invalid user design length!\r\n",
                        sizeof("[ERROR] Invalid user design length!\r\n") - 1U);
        return HAL_ERROR;
    }

    CDC_Transmit_FS((uint8_t*)"[FLASH] Load SPI bridge bitstream...\r\n", 38);
    osDelay(1);
		HAL_StatusTypeDef ret = Jtag_ConfigureFromBuffer(g_spi_bridge_bin, g_spi_bridge_bin_len);
    if(ret != HAL_OK) {
        CDC_Transmit_FS((uint8_t*)"[ERROR] Bridge load failed!\r\n", 30);
        return ret;
    }

    osDelay(100);
    memset(&g_jtag_xfer, 0, sizeof(g_jtag_xfer));
    Jtag_Init(&jtag, hal, &g_jtag_xfer);

    if (spi_flash_init(&jtag) != HAL_OK) {
        CDC_Transmit_FS((uint8_t*)"[ERROR] SPI bridge init failed!\r\n",
                        sizeof("[ERROR] SPI bridge init failed!\r\n") - 1U);
        return HAL_ERROR;
    }
    CDC_Transmit_FS((uint8_t*)"[FLASH] SPI controller init done\r\n", 34);

    if (Bridge_ReadStatusEx(&jtag, bridge_status) == HAL_OK) {
        log_len = (uint16_t)snprintf(log_buf, sizeof(log_buf),
                                     "[BRIDGE] EX flags=0x%02X state_idx=0x%02X sck=0x%02X%02X\r\n",
                                     bridge_status[0], bridge_status[1],
                                     bridge_status[3], bridge_status[2]);
        CDC_Transmit_FS((uint8_t*)log_buf, log_len);
    }

    if (spi_flash_reset_memory(&jtag) != HAL_OK) {
        CDC_Transmit_FS((uint8_t*)"[ERROR] Flash software reset failed!\r\n",
                        sizeof("[ERROR] Flash software reset failed!\r\n") - 1U);
        return HAL_ERROR;
    }
    CDC_Transmit_FS((uint8_t*)"[FLASH] Flash software reset done\r\n",
                    sizeof("[FLASH] Flash software reset done\r\n") - 1U);

    if (Bridge_ReadStatusEx(&jtag, bridge_status) == HAL_OK) {
        log_len = (uint16_t)snprintf(log_buf, sizeof(log_buf),
                                     "[BRIDGE] EX after reset flags=0x%02X state_idx=0x%02X sck=0x%02X%02X\r\n",
                                     bridge_status[0], bridge_status[1],
                                     bridge_status[3], bridge_status[2]);
        CDC_Transmit_FS((uint8_t*)log_buf, log_len);
    }

    if (spi_flash_read_status(&jtag, &status_reg) != HAL_OK) {
        CDC_Transmit_FS((uint8_t*)"[ERROR] Status register read failed!\r\n",
                        sizeof("[ERROR] Status register read failed!\r\n") - 1U);
        return HAL_ERROR;
    }

    if (spi_flash_read_flag_status(&jtag, &flag_status_reg) != HAL_OK) {
        CDC_Transmit_FS((uint8_t*)"[ERROR] Flag status register read failed!\r\n",
                        sizeof("[ERROR] Flag status register read failed!\r\n") - 1U);
        return HAL_ERROR;
    }

    log_len = (uint16_t)snprintf(log_buf, sizeof(log_buf),
                                 "[FLASH] SR=0x%02X FSR=0x%02X\r\n",
                                 status_reg, flag_status_reg);
    CDC_Transmit_FS((uint8_t*)log_buf, log_len);

    if (spi_flash_read_jedec_id(&jtag, jedec_id) != HAL_OK) {
        CDC_Transmit_FS((uint8_t*)"[ERROR] Flash JEDEC ID read failed!\r\n", 37);
        return HAL_ERROR;
    }

    if (Bridge_ReadStatusEx(&jtag, bridge_status) == HAL_OK) {
        log_len = (uint16_t)snprintf(log_buf, sizeof(log_buf),
                                     "[BRIDGE] EX after JEDEC flags=0x%02X state_idx=0x%02X sck=0x%02X%02X\r\n",
                                     bridge_status[0], bridge_status[1],
                                     bridge_status[3], bridge_status[2]);
        CDC_Transmit_FS((uint8_t*)log_buf, log_len);
    }

    if (((jedec_id[0] == 0x00U) && (jedec_id[1] == 0x00U) && (jedec_id[2] == 0x00U)) ||
        ((jedec_id[0] == 0xFFU) && (jedec_id[1] == 0xFFU) && (jedec_id[2] == 0xFFU))) {
        CDC_Transmit_FS((uint8_t*)"[ERROR] Flash JEDEC ID looks invalid!\r\n",
                        sizeof("[ERROR] Flash JEDEC ID looks invalid!\r\n") - 1U);
        return HAL_ERROR;
    }

    log_len = snprintf(log_buf, sizeof(log_buf),
                       "[FLASH] JEDEC ID: %02X %02X %02X\r\n",
                       jedec_id[0], jedec_id[1], jedec_id[2]);
    CDC_Transmit_FS((uint8_t*)log_buf, log_len);

    uint32_t total_sectors = (bin_len + (SPI_FLASH_SECTOR_SIZE - 1U)) / SPI_FLASH_SECTOR_SIZE;
    log_len = snprintf(log_buf, sizeof(log_buf),
                       "[FLASH] Total sectors to erase: %" PRIu32 "\r\n",
                       total_sectors);
    CDC_Transmit_FS((uint8_t*)log_buf, log_len);

    for (uint32_t i = 0; i < total_sectors; i++) {
        ret = spi_flash_erase_sector(&jtag, i * SPI_FLASH_SECTOR_SIZE);
        if (ret != HAL_OK) {
            log_len = snprintf(log_buf, sizeof(log_buf),
                               "[ERROR] Erase failed at sector 0x%08" PRIX32 "\r\n",
                               i * SPI_FLASH_SECTOR_SIZE);
            CDC_Transmit_FS((uint8_t*)log_buf, log_len);
            return ret;
        }

        if((i % 16U) == 0U) {
            log_len = snprintf(log_buf, sizeof(log_buf),
                               "[FLASH] Erase progress: %" PRIu32 "/%" PRIu32 "\r\n",
                               i + 1U, total_sectors);
            CDC_Transmit_FS((uint8_t*)log_buf, log_len);
        }
    }
    CDC_Transmit_FS((uint8_t*)"[FLASH] Erase complete!\r\n", 26);

    uint32_t total_pages = (bin_len + (SPI_FLASH_PAGE_SIZE - 1U)) / SPI_FLASH_PAGE_SIZE;
    log_len = snprintf(log_buf, sizeof(log_buf),
                       "[FLASH] Total pages to program: %" PRIu32 "\r\n",
                       total_pages);
    CDC_Transmit_FS((uint8_t*)log_buf, log_len);

    for (uint32_t i = 0; i < total_pages; i++) {
        uint32_t page_addr = i * SPI_FLASH_PAGE_SIZE;
        uint16_t write_len = (i == (total_pages - 1U)) ? (uint16_t)(bin_len - page_addr) : SPI_FLASH_PAGE_SIZE;
        ret = spi_flash_program_page(&jtag, page_addr, &bin_data[page_addr], write_len);
        if (ret != HAL_OK) {
            log_len = snprintf(log_buf, sizeof(log_buf),
                               "[ERROR] Program failed at 0x%08" PRIX32 "\r\n",
                               page_addr);
            CDC_Transmit_FS((uint8_t*)log_buf, log_len);
            return ret;
        }

        if((i % 64U) == 0U)
        {
            log_len = snprintf(log_buf, sizeof(log_buf), "[FLASH] Program progress: %.1f%%\r\n", (float)i / total_pages * 100.0f);
            CDC_Transmit_FS((uint8_t*)log_buf, log_len);
        }
    }
    CDC_Transmit_FS((uint8_t*)"[FLASH] Program complete!\r\n", 28);

    CDC_Transmit_FS((uint8_t*)"[FLASH] Start data verify...\r\n", 32);
    for(uint32_t i = 0; i < total_pages; i++)
    {
        uint32_t page_addr = i * SPI_FLASH_PAGE_SIZE;
        uint16_t read_len = (i == (total_pages - 1U)) ? (uint16_t)(bin_len - page_addr) : SPI_FLASH_PAGE_SIZE;

        if (spi_flash_read_buffer(&jtag, page_addr, verify_buf, read_len) != HAL_OK)
        {
            log_len = snprintf(log_buf, sizeof(log_buf),
                               "[ERROR] Verify read failed at 0x%08X\r\n",
                               page_addr);
            CDC_Transmit_FS((uint8_t*)log_buf, log_len);
            return HAL_ERROR;
        }

        if (memcmp(verify_buf, &bin_data[page_addr], read_len) != 0)
        {
            uint16_t failed_index = 0U;
            while ((failed_index < read_len) && (verify_buf[failed_index] == bin_data[page_addr + failed_index]))
            {
                failed_index++;
            }

            log_len = snprintf(log_buf, sizeof(log_buf),
                               "[ERROR] Verify failed at 0x%08X: 0x%02X vs 0x%02X\r\n",
                               page_addr + failed_index,
                               verify_buf[failed_index],
                               bin_data[page_addr + failed_index]);
            CDC_Transmit_FS((uint8_t*)log_buf, log_len);
            return HAL_ERROR;
        }
    }
    CDC_Transmit_FS((uint8_t*)"[FLASH] Verify success!\r\n", 26);

    CDC_Transmit_FS((uint8_t*)"[FLASH] Reset FPGA, load from Flash...\r\n", 40);
    FPGA_Reset();

    if(FPGA_Wait_InitB_Ready() != HAL_OK)
    {
        CDC_Transmit_FS((uint8_t*)"[ERROR] INIT_B timeout after Flash boot reset!\r\n", 46);
        return HAL_TIMEOUT;
    }

    if(FPGA_Wait_DONE_High() != HAL_OK)
    {
        CDC_Transmit_FS((uint8_t*)"[ERROR] FPGA load from Flash failed!\r\n", 38);
        return HAL_ERROR;
    }

    CDC_Transmit_FS((uint8_t*)"[FLASH] All done! FPGA boot success!\r\n", 38);
    return HAL_OK;
}
