/*
 * File: FPGAConfigDefaultTask.c
 */

#include "FPGAConfigDefaultTask.h"
#include "fpga_config.h"
#include "usb_device.h"
#include "cmsis_os.h"
#include "sdram.h"

#define LCD_STUTTER_ISOLATE_FPGA_TASK 0U

uint8_t g_usb_recv_flag = 0;
static uint8_t g_log_buf[128] = {0};
static uint32_t g_log_len = 0;
static uint8_t first_call = 1;
volatile uint8_t g_sdram_ready = 0U;
uint8_t g_fpgamode = 0;
uint8_t g_wait_mode_flag = 0;
static uint8_t g_skip_optional_start_marker = 0U;
static volatile FPGA_UI_FlowState_t g_fpga_ui_flow = FPGA_UI_FLOW_IDLE;
static volatile uint8_t g_fpga_ui_abort_requested = 0U;
static void FPGA_ResetBinReceiveState(void);
static uint32_t FPGA_NormalizeAsciiCommand(const uint8_t *src, uint32_t len, char *dst, uint32_t dst_size);
static int FPGA_CommandEquals(const uint8_t *buf, uint32_t len, const char *token);

static void FPGA_UI_SetFlow(FPGA_UI_FlowState_t state)
{
    g_fpga_ui_flow = state;
}

void FPGA_UI_ResetSession(void)
{
    FPGA_ResetBinReceiveState();
    g_fpgamode = 0U;
    g_sdram_recv_state = SDRAM_RECV_IDLE;
    g_skip_optional_start_marker = 0U;
    g_fpga_ui_abort_requested = 0U;
    FPGA_UI_SetFlow(FPGA_UI_FLOW_IDLE);
}

static void FPGA_ResetBinReceiveState(void)
{
    SDRAM_Bin_Cache_Reset();
    g_usb_recv_flag = 0U;
    g_fpga_config_start = 0U;
    g_wait_mode_flag = 0U;
    first_call = 1U;
}

static uint32_t FPGA_NormalizeAsciiCommand(const uint8_t *src, uint32_t len, char *dst, uint32_t dst_size)
{
    uint32_t i;
    uint32_t out_len = 0U;

    if ((src == NULL) || (dst == NULL) || (dst_size == 0U)) {
        return 0U;
    }

    for (i = 0U; i < len; i++) {
        uint8_t ch = src[i];

        if ((ch == ' ') || (ch == '\r') || (ch == '\n') || (ch == '\t')) {
            continue;
        }

        if ((ch >= 'a') && (ch <= 'z')) {
            ch = (uint8_t)(ch - ('a' - 'A'));
        }

        if (out_len + 1U >= dst_size) {
            break;
        }

        dst[out_len++] = (char)ch;
    }

    dst[out_len] = '\0';
    return out_len;
}

static int FPGA_CommandEquals(const uint8_t *buf, uint32_t len, const char *token)
{
    char normalized[24];

    if (token == NULL) {
        return 0;
    }

    FPGA_NormalizeAsciiCommand(buf, len, normalized, sizeof(normalized));
    return (strcmp(normalized, token) == 0) ? 1 : 0;
}

static void FPGA_UI_CancelPendingSession(const char *reason)
{
    FPGA_UI_ResetSession();
    g_log_len = snprintf((char*)g_log_buf, sizeof(g_log_buf),
                         "[ABORT] %s\r\n", (reason != NULL) ? reason : "Configuration session canceled.");
}

static void FPGA_BeginModeSelection(void)
{
    FPGA_ResetBinReceiveState();
    g_fpgamode = 0U;
    g_sdram_recv_state = SDRAM_RECV_IDLE;
    g_skip_optional_start_marker = 0U;
    g_wait_mode_flag = 1U;
    FPGA_UI_SetFlow(FPGA_UI_FLOW_WAIT_MODE);
    g_log_len = snprintf((char*)g_log_buf, sizeof(g_log_buf),
                         "[WAIT] Send Mode: 0x01=PS 0x02=JTAG 0x03=FLASH\r\n");
}

static void FPGA_BeginBinReceive(void)
{
    FPGA_ResetBinReceiveState();
    g_sdram_recv_state = SDRAM_RECV_DATA;
    g_skip_optional_start_marker = 0U;
    FPGA_UI_SetFlow(FPGA_UI_FLOW_WAIT_BIN);
    g_log_len = snprintf((char*)g_log_buf, sizeof(g_log_buf),
                         "[READY] Bin File Recv Ready!\r\n");
}

void FPGA_UI_SelectMode(FPGA_UI_Mode_t mode)
{
    if ((mode != FPGA_UI_MODE_SLAVE_SERIAL) &&
        (mode != FPGA_UI_MODE_JTAG_SRAM) &&
        (mode != FPGA_UI_MODE_JTAG_FLASH)) {
        return;
    }

    g_fpgamode = (uint8_t)mode;
    FPGA_BeginBinReceive();
    g_skip_optional_start_marker = 1U;
}

FPGA_UI_Mode_t FPGA_UI_GetMode(void)
{
    return (FPGA_UI_Mode_t)g_fpgamode;
}

FPGA_UI_FlowState_t FPGA_UI_GetFlowState(void)
{
    return g_fpga_ui_flow;
}

uint8_t FPGA_UI_IsAbortRequested(void)
{
    return (uint8_t)g_fpga_ui_abort_requested;
}

uint32_t FPGA_UI_GetBinSize(void)
{
    return g_sdram_bin_offset;
}

void FPGA_UI_RequestStart(void)
{
    if ((g_fpga_ui_flow == FPGA_UI_FLOW_BIN_DONE_WAIT_START) &&
        ((g_fpgamode == 1U) || (g_fpgamode == 2U) || (g_fpgamode == 3U))) {
        g_fpga_config_start = 1U;
    }
}

void FPGA_UI_RequestAbort(void)
{
    if (g_fpga_ui_flow == FPGA_UI_FLOW_CONFIGURING) {
        g_fpga_ui_abort_requested = 1U;
        FPGA_UI_SetFlow(FPGA_UI_FLOW_ABORTING);
        g_log_len = snprintf((char*)g_log_buf, sizeof(g_log_buf),
                             "[ABORT] Stop requested, leaving configuration mode...\r\n");
        return;
    }

    FPGA_UI_ResetSession();
    g_log_len = snprintf((char*)g_log_buf, sizeof(g_log_buf),
                         "[ABORT] Configuration session canceled.\r\n");
}

static void SDRAM_Process_Data_Block(uint8_t* buf, uint32_t len)
{
    static uint8_t tail[4] = {0};
    static uint8_t tail_len = 0;
    static const uint8_t end_marker[4] = {
        CMD_END_BIN_BYTE1,
        CMD_END_BIN_BYTE2,
        CMD_END_BIN_BYTE3,
        CMD_END_BIN_BYTE4
    };

    if(first_call)
    {
        g_sdram_bin_offset = 0;
        memset(tail, 0, sizeof(tail));
        tail_len = 0;
        first_call = 0;
    }

    for(uint32_t i = 0; i < len; i++)
    {
        uint8_t curr = buf[i];

        if(tail_len < sizeof(tail))
        {
            tail[tail_len++] = curr;
            if((tail_len == sizeof(tail)) && (memcmp(tail, end_marker, sizeof(tail)) == 0))
            {
                g_sdram_recv_state = SDRAM_RECV_COMPLETE;
                tail_len = 0;
                return;
            }
            continue;
        }

        if(g_sdram_recv_state == SDRAM_RECV_DATA)
        {
            if(g_sdram_bin_offset < SDRAM_TOTAL_SIZE)
            {
                SDRAM_WriteBuffer(&tail[0], g_sdram_bin_offset, 1);
                g_sdram_bin_offset++;
            }
        }

        memmove(&tail[0], &tail[1], sizeof(tail) - 1U);
        tail[sizeof(tail) - 1U] = curr;

        if(memcmp(tail, end_marker, sizeof(tail)) == 0)
        {
            g_sdram_recv_state = SDRAM_RECV_COMPLETE;
            tail_len = 0;
            return;
        }
    }
}

int8_t USB_CDC_Recv_Callback(uint8_t* buf, uint32_t* len)
{
    uint32_t i;
    uint32_t payload_offset = 0U;
    FPGA_UI_FlowState_t flow;

    if((*len == 0U) || (buf == NULL))
    {
        return USBD_OK;
    }

    flow = FPGA_UI_GetFlowState();

    if (((*len == 4U) &&
         (buf[0] == 0xAAU) &&
         (buf[1] == 0x55U) &&
         (buf[2] == 0x55U) &&
         (buf[3] == 0xAAU) &&
         (flow != FPGA_UI_FLOW_CONFIGURING) &&
         (flow != FPGA_UI_FLOW_ABORTING)))
    {
        if (flow != FPGA_UI_FLOW_IDLE)
        {
            FPGA_UI_CancelPendingSession("Configuration session canceled by host.");
        }

        g_usb_recv_flag = 1U;
        return USBD_OK;
    }

    if ((flow == FPGA_UI_FLOW_BIN_DONE_WAIT_START) &&
        FPGA_CommandEquals(buf, *len, "1231"))
    {
        FPGA_UI_RequestStart();
        g_usb_recv_flag = 1U;
        return USBD_OK;
    }

    if (flow == FPGA_UI_FLOW_BIN_DONE_WAIT_START)
    {
        FPGA_Check_Config_Cmd(buf, *len);
    }

    if ((((*len == 1U) && (buf[0] == CMD_START_BIN)) ||
         FPGA_CommandEquals(buf, *len, "5A")) &&
        (flow != FPGA_UI_FLOW_CONFIGURING) &&
        (flow != FPGA_UI_FLOW_ABORTING))
    {
        if ((g_fpgamode == (uint8_t)FPGA_UI_MODE_SLAVE_SERIAL) ||
            (g_fpgamode == (uint8_t)FPGA_UI_MODE_JTAG_SRAM) ||
            (g_fpgamode == (uint8_t)FPGA_UI_MODE_JTAG_FLASH))
        {
            FPGA_BeginBinReceive();
            g_skip_optional_start_marker = 1U;
        }
        else
        {
            FPGA_BeginModeSelection();
        }

        g_usb_recv_flag = 1U;
        return USBD_OK;
    }

    if (g_wait_mode_flag == 1U)
    {
        uint8_t selected_mode = 0U;

        if (*len == 1U)
        {
            if ((buf[0] == 0x01U) || (buf[0] == 0x02U) || (buf[0] == 0x03U))
            {
                selected_mode = buf[0];
            }
        }
        else if (FPGA_CommandEquals(buf, *len, "01"))
        {
            selected_mode = 0x01U;
        }
        else if (FPGA_CommandEquals(buf, *len, "02"))
        {
            selected_mode = 0x02U;
        }
        else if (FPGA_CommandEquals(buf, *len, "03"))
        {
            selected_mode = 0x03U;
        }

        if(selected_mode != 0U)
        {
            const char *mode_name = "Unknown";

            g_fpgamode = selected_mode;

            if(g_fpgamode == 0x01U)
            {
                mode_name = "Slave Serial";
            }
            else if(g_fpgamode == 0x02U)
            {
                mode_name = "JTAG";
            }
            else if(g_fpgamode == 0x03U)
            {
                mode_name = "JTAG Flash";
            }

            g_wait_mode_flag = 0U;
            FPGA_BeginBinReceive();
            g_log_len = snprintf((char*)g_log_buf, sizeof(g_log_buf),
                                 "[INFO] Mode Selected: %s\r\n[READY] Bin File Recv Ready!\r\n", mode_name);
            return USBD_OK;
        }
    }

    if (g_sdram_recv_state == SDRAM_RECV_DATA)
    {
        if (FPGA_CommandEquals(buf, *len, "55AAAA55"))
        {
            g_sdram_recv_state = SDRAM_RECV_COMPLETE;
            g_usb_recv_flag = 1U;
            return USBD_OK;
        }

        if ((g_skip_optional_start_marker != 0U) &&
            (((*len > 0U) && (buf[0] == CMD_START_BIN)) ||
             FPGA_CommandEquals(buf, *len, "5A")))
        {
            g_skip_optional_start_marker = 0U;
            payload_offset = ((*len == 1U) && (buf[0] == CMD_START_BIN)) ? 1U : *len;
        }
        else
        {
            g_skip_optional_start_marker = 0U;
        }

        if (payload_offset < *len)
        {
            SDRAM_Process_Data_Block(&buf[payload_offset], *len - payload_offset);
        }

        g_usb_recv_flag = 1U;
        return USBD_OK;
    }

    if (g_sdram_recv_state == SDRAM_RECV_IDLE)
    {
        for (i = 0U; i < *len; i++)
        {
            if (buf[i] == CMD_START_BIN)
            {
                FPGA_BeginModeSelection();
                break;
            }
        }
    }

    g_usb_recv_flag = 1;
    return USBD_OK;
}

void FPGAConfigDefaultTask(void const * argument)
{
    (void)argument;

    if(g_sdram_ready == 0U)
    {
        /* FMC base init is already done in main(); only run the SDRAM command sequence here once. */
        SDRAM_Init_Sequence();
        g_sdram_ready = 1U;
    }
    FPGA_UI_ResetSession();

#if (LCD_STUTTER_ISOLATE_FPGA_TASK == 1U)
    for (;;)
    {
        osDelay(1000);
    }
#endif

    /* Give the USB host time to see a clean detach/attach sequence when not under debugger. */
    osDelay(800);
    MX_USB_DEVICE_Init();

#if CONFIGURATION_MODE
    MX_SPI4_Init();
#endif

    osDelay(200);
    HAL_GPIO_WritePin(FPGA_PROGB_PORT, FPGA_PROGB_PIN, GPIO_PIN_SET);

    for(;;)
    {
        if(g_log_len > 0U)
        {
            CDC_Transmit_FS(g_log_buf, g_log_len);
            g_log_len = 0;
            osDelay(5);
        }

        if(g_sdram_recv_state == SDRAM_RECV_COMPLETE)
        {
            g_log_len = snprintf((char*)g_log_buf, sizeof(g_log_buf),
                                 "[INFO] Bin File Recv Complete! Total Size: %.2f MB\r\n",
                                 (float)g_sdram_bin_offset / 1024.0f / 1024.0f);
            CDC_Transmit_FS(g_log_buf, g_log_len);
            g_log_len = 0;
            osDelay(10);

            if((g_fpgamode == 1U) || (g_fpgamode == 2U) || (g_fpgamode == 3U))
            {
                FPGA_UI_SetFlow(FPGA_UI_FLOW_BIN_DONE_WAIT_START);
                g_log_len = snprintf((char*)g_log_buf, sizeof(g_log_buf),
                                     "[READY] Send 0x1231 to start config\r\n");
                CDC_Transmit_FS(g_log_buf, g_log_len);
                g_log_len = 0;
                g_wait_mode_flag = 0U;
            }
            else
            {
                FPGA_BeginModeSelection();
                CDC_Transmit_FS(g_log_buf, g_log_len);
                g_log_len = 0U;
            }

            g_sdram_recv_state = SDRAM_RECV_IDLE;
            first_call = 1U;
            osDelay(10);
        }

        if(g_fpga_config_start == 1U)
        {
            HAL_StatusTypeDef ret = HAL_ERROR;

            FPGA_UI_SetFlow(FPGA_UI_FLOW_CONFIGURING);
            g_fpga_ui_abort_requested = 0U;

            if((g_fpgamode != 1U) && (g_fpgamode != 2U) && (g_fpgamode != 3U))
            {
                CDC_Transmit_FS((uint8_t*)"[ERROR] Please select mode first!\r\n", 35);
                g_fpga_config_start = 0;
                FPGA_UI_SetFlow(FPGA_UI_FLOW_WAIT_MODE);
                osDelay(10);
                continue;
            }

            CDC_Transmit_FS((uint8_t*)"[FPGA] Start FPGA configuration...\r\n", 36);

            if(g_fpgamode == 1U)
            {
                FPGA_Switch_Mode(FPGA_MODE_SLAVE_SERIAL);
                ret = FPGA_Send_Bin_From_SDRAM(g_sdram_bin_offset);
            }
            else if(g_fpgamode == 2U)
            {
                FPGA_Switch_Mode(FPGA_MODE_JTAG);
                ret = Jtag_ConfigureFromSdram(g_sdram_bin_offset);
            }
            else if(g_fpgamode == 3U)
            {
                CDC_Transmit_FS((uint8_t*)"[FLASH] Start SPI Flash programming...\r\n", 40);
								osDelay(1);
                FPGA_Switch_Mode(FPGA_MODE_JTAG);
                ret = spi_flash_program_full((uint8_t*)SDRAM_BASE_ADDR, g_sdram_bin_offset);
            }
            else
            {
                CDC_Transmit_FS((uint8_t*)"[ERROR] Invalid mode selected!\r\n", 32);
                g_fpga_config_start = 0;
                FPGA_UI_SetFlow(FPGA_UI_FLOW_WAIT_MODE);
                osDelay(10);
                continue;
            }

            if (g_fpga_ui_abort_requested != 0U)
            {
                FPGA_Reset();
                FPGA_UI_ResetSession();
                CDC_Transmit_FS((uint8_t*)"[FPGA] Configuration aborted by EC2.\r\n", 39);
            }
            else if(ret == HAL_OK)
            {
                CDC_Transmit_FS((uint8_t*)"[FPGA] Configuration success!\r\n", 30);
                FPGA_UI_SetFlow(FPGA_UI_FLOW_SUCCESS);
            }
            else
            {
                CDC_Transmit_FS((uint8_t*)"[FPGA] Configuration failed!\r\n", 30);
                FPGA_UI_SetFlow(FPGA_UI_FLOW_FAILED);
            }

            osDelay(1);
            g_fpga_config_start = 0;
            if (FPGA_UI_GetFlowState() != FPGA_UI_FLOW_IDLE)
            {
                g_log_len = snprintf((char*)g_log_buf, sizeof(g_log_buf),
                                     "[READY] Wait next start cmd (0x5A)\r\n");
            }
            osDelay(1);
        }

        osDelay(10);
    }
}


