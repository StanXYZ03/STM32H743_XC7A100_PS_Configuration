/*
 * File: FPGAConfigDefaultTask.c
 */

#include "FPGAConfigDefaultTask.h"
#include "fpga_config.h"
#include "usb_device.h"
#include "cmsis_os.h"
#include "sdram.h"

uint8_t g_usb_recv_flag = 0;
static uint8_t g_log_buf[128] = {0};
static uint32_t g_log_len = 0;
static uint8_t first_call = 1;
uint8_t g_fpgamode = 0;
uint8_t g_wait_mode_flag = 0;

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
    uint32_t i = 0;

    if((*len == 0U) || (buf == NULL))
    {
        return USBD_OK;
    }

    FPGA_Check_Config_Cmd(buf, *len);

    if((g_wait_mode_flag == 1U) && (*len == 1U))
    {
        if((buf[0] == 0x01U) || (buf[0] == 0x02U) || (buf[0] == 0x03U))
        {
            const char *mode_name = "Unknown";

            g_fpgamode = buf[0];
            g_wait_mode_flag = 0;

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

            snprintf((char*)g_log_buf, sizeof(g_log_buf),
                     "[INFO] Mode Selected: %s\r\n", mode_name);
            CDC_Transmit_FS(g_log_buf, strlen((char*)g_log_buf));

            snprintf((char*)g_log_buf, sizeof(g_log_buf),
                     "[READY] Send 0x1231 to start config\r\n");
            CDC_Transmit_FS(g_log_buf, strlen((char*)g_log_buf));
            return USBD_OK;
        }
    }

    for(i = 0; i < *len; i++)
    {
        if(g_sdram_recv_state == SDRAM_RECV_IDLE)
        {
            if(buf[i] == CMD_START_BIN)
            {
                SDRAM_Bin_Cache_Reset();
                g_sdram_recv_state = SDRAM_RECV_DATA;
                g_log_len = snprintf((char*)g_log_buf, sizeof(g_log_buf),
                                     "[READY] Bin File Recv Ready!\r\n");
                i++;
                break;
            }
            else
            {
                continue;
            }
        }
        else
        {
            break;
        }
    }

    if((i < *len) && (g_sdram_recv_state == SDRAM_RECV_DATA))
    {
        SDRAM_Process_Data_Block(&buf[i], *len - i);
    }

    g_usb_recv_flag = 1;
    return USBD_OK;
}

void FPGAConfigDefaultTask(void const * argument)
{
    (void)argument;

    MX_FMC_Init();
    SDRAM_Init_Sequence();
    SDRAM_Bin_Cache_Reset();

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

            g_fpgamode = 0;
            g_wait_mode_flag = 1;

            g_log_len = snprintf((char*)g_log_buf, sizeof(g_log_buf),
                                 "[WAIT] Send Mode: 0x01=PS 0x02=JTAG 0x03=FLASH\r\n");
            CDC_Transmit_FS(g_log_buf, g_log_len);
            g_log_len = 0;

            g_sdram_recv_state = SDRAM_RECV_IDLE;
            first_call = 1;
            osDelay(10);
        }

        if(g_fpga_config_start == 1U)
        {
            HAL_StatusTypeDef ret = HAL_ERROR;

            if((g_fpgamode != 1U) && (g_fpgamode != 2U) && (g_fpgamode != 3U))
            {
                CDC_Transmit_FS((uint8_t*)"[ERROR] Please select mode first!\r\n", 35);
                g_fpga_config_start = 0;
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
                osDelay(10);
                continue;
            }

            if(ret == HAL_OK)
            {
                CDC_Transmit_FS((uint8_t*)"[FPGA] Configuration success!\r\n", 30);
            }
            else
            {
                CDC_Transmit_FS((uint8_t*)"[FPGA] Configuration failed!\r\n", 30);
            }

            osDelay(1);
            g_fpga_config_start = 0;
            g_log_len = snprintf((char*)g_log_buf, sizeof(g_log_buf),
                                 "[READY] Wait next start cmd (0x5A)\r\n");
            osDelay(1);
        }

        osDelay(10);
    }
}
