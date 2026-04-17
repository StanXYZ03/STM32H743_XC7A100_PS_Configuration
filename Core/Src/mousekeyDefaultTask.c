#include "mousekeyDefaultTask.h"

#include "spi.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"
#include "usbd_cdc.h"
#include "FPGAConfigDefaultTask.h"
#include "bsp_dwt.h"
#include "cmsis_os.h"

#include <stdlib.h>
#include <string.h>

extern USBD_HandleTypeDef hUsbDeviceHS;

#define MOUSEKEY_CS_DATA_PORT         GPIOA
#define MOUSEKEY_CS_DATA_PIN          GPIO_PIN_15
#define MOUSEKEY_CS_CMD_PORT          GPIOI
#define MOUSEKEY_CS_CMD_PIN           GPIO_PIN_3

#define MOUSEKEY_SPI_TIMEOUT_MS       20U
#define MOUSEKEY_CS_SETUP_US          2U
#define MOUSEKEY_CMD_DATA_GAP_US      2U

#define MOUSEKEY_CMD_WIDTH_BYTES      1U
#define MOUSEKEY_DATA_WIDTH_BYTES     4U

#define MOUSEKEY_SCREEN_WIDTH         1920
#define MOUSEKEY_SCREEN_HEIGHT        1080
#define MOUSEKEY_SENSITIVITY_PERCENT  50
#define MOUSEKEY_OFFSET_THRESHOLD     1
#define MOUSEKEY_SCALE_X_COEFF        ((MOUSEKEY_SCREEN_WIDTH * 100) / 256)
#define MOUSEKEY_SCALE_Y_COEFF        ((MOUSEKEY_SCREEN_HEIGHT * 100) / 256)

#define MOUSEKEY_REG_MOUSE_FRAME      0x00000101UL
#define MOUSEKEY_REG_VALID_MASK       0xFFU
#define MOUSEKEY_REG_TYPE_SHIFT       8U
#define MOUSEKEY_REG_KEYBOARD_TYPE    0x00U
#define MOUSEKEY_REG_KEYBOARD_VALID   0x01U

typedef struct
{
    uint8_t modifier;
    uint8_t key_codes[6];
} MouseKeyKeyboardState_t;

MouseKeyPacket_t g_mousekey_packet = {
    .header = 0x55U,
    .mouse_ctrl = 0U,
    .mouse_wheel = 0U,
    .x_coord = MOUSEKEY_SCREEN_WIDTH / 2,
    .y_coord = MOUSEKEY_SCREEN_HEIGHT / 2,
    .key_mod = 0U,
    .key_code = {0U}
};

volatile int16_t g_mousekey_x = MOUSEKEY_SCREEN_WIDTH / 2;
volatile int16_t g_mousekey_y = MOUSEKEY_SCREEN_HEIGHT / 2;
volatile uint32_t g_mousekey_debug_regs[3] = {0U, 0U, 0U};
volatile uint32_t g_mousekey_debug_loop_count = 0U;
volatile uint32_t g_mousekey_debug_ok_count = 0U;
volatile uint32_t g_mousekey_debug_fail_count = 0U;
volatile uint8_t g_mousekey_debug_last_status = 0U;

static uint8_t g_mousekey_tx_buf[MOUSEKEY_USB_PACKET_LEN];
static uint32_t g_last_mouse_data = 0U;
static uint8_t g_last_wheel_val = 0U;
static uint8_t g_last_ctrl_data = 0U;
static uint8_t g_mouse_data_valid = 0U;

static uint8_t MouseKey_IsTxMutedByConfigFlow(void)
{
    return (uint8_t)(FPGA_UI_GetFlowState() != FPGA_UI_FLOW_IDLE);
}

static void MouseKey_SPI_ConfigSlow(void)
{
    if (hspi1.Init.BaudRatePrescaler == SPI_BAUDRATEPRESCALER_256)
    {
        return;
    }

    if (HAL_SPI_DeInit(&hspi1) != HAL_OK)
    {
        Error_Handler();
    }

    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
    hspi1.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
    hspi1.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
    hspi1.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
    hspi1.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
    hspi1.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
    hspi1.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
    hspi1.Init.IOSwap = SPI_IO_SWAP_DISABLE;

    if (HAL_SPI_Init(&hspi1) != HAL_OK)
    {
        Error_Handler();
    }
}

static void MouseKey_CS_AllHigh(void)
{
    HAL_GPIO_WritePin(MOUSEKEY_CS_CMD_PORT, MOUSEKEY_CS_CMD_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(MOUSEKEY_CS_DATA_PORT, MOUSEKEY_CS_DATA_PIN, GPIO_PIN_SET);
}

static void MouseKey_CS_SelectCmd(void)
{
    HAL_GPIO_WritePin(MOUSEKEY_CS_DATA_PORT, MOUSEKEY_CS_DATA_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(MOUSEKEY_CS_CMD_PORT, MOUSEKEY_CS_CMD_PIN, GPIO_PIN_RESET);
    bsp_DelayUS(MOUSEKEY_CS_SETUP_US);
}

static void MouseKey_CS_SelectData(void)
{
    HAL_GPIO_WritePin(MOUSEKEY_CS_CMD_PORT, MOUSEKEY_CS_CMD_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(MOUSEKEY_CS_DATA_PORT, MOUSEKEY_CS_DATA_PIN, GPIO_PIN_RESET);
    bsp_DelayUS(MOUSEKEY_CS_SETUP_US);
}

static HAL_StatusTypeDef MouseKey_SPI_SendCmd(uint8_t cmd)
{
    HAL_StatusTypeDef status;

    MouseKey_CS_SelectCmd();
    status = HAL_SPI_Transmit(&hspi1, &cmd, MOUSEKEY_CMD_WIDTH_BYTES, MOUSEKEY_SPI_TIMEOUT_MS);
    bsp_DelayUS(MOUSEKEY_CS_SETUP_US);
    MouseKey_CS_AllHigh();

    return status;
}

static HAL_StatusTypeDef MouseKey_SPI_ReadDataWord(uint32_t *value)
{
    HAL_StatusTypeDef status;
    uint8_t tx_dummy[MOUSEKEY_DATA_WIDTH_BYTES] = {0U, 0U, 0U, 0U};
    uint8_t rx_data[MOUSEKEY_DATA_WIDTH_BYTES] = {0U, 0U, 0U, 0U};

    if (value == NULL)
    {
        return HAL_ERROR;
    }

    MouseKey_CS_SelectData();
    status = HAL_SPI_TransmitReceive(&hspi1,
                                     tx_dummy,
                                     rx_data,
                                     MOUSEKEY_DATA_WIDTH_BYTES,
                                     MOUSEKEY_SPI_TIMEOUT_MS);
    bsp_DelayUS(MOUSEKEY_CS_SETUP_US);
    MouseKey_CS_AllHigh();

    if (status == HAL_OK)
    {
        *value = ((uint32_t)rx_data[0] << 24) |
                 ((uint32_t)rx_data[1] << 16) |
                 ((uint32_t)rx_data[2] << 8) |
                 ((uint32_t)rx_data[3]);
    }

    return status;
}

static HAL_StatusTypeDef MouseKey_SPI_ReadRegister(uint8_t cmd, uint32_t *value)
{
    uint32_t dummy = 0U;

    if (value == NULL)
    {
        return HAL_ERROR;
    }

    /* Keep the original F4 software-SPI read pipeline for FPGA compatibility. */
    if (cmd == 0U)
    {
        if (MouseKey_SPI_SendCmd(0U) != HAL_OK)
        {
            return HAL_ERROR;
        }
        bsp_DelayUS(MOUSEKEY_CMD_DATA_GAP_US);
        if (MouseKey_SPI_ReadDataWord(&dummy) != HAL_OK)
        {
            return HAL_ERROR;
        }
        bsp_DelayUS(MOUSEKEY_CMD_DATA_GAP_US);
        if (MouseKey_SPI_SendCmd(1U) != HAL_OK)
        {
            return HAL_ERROR;
        }
        bsp_DelayUS(MOUSEKEY_CMD_DATA_GAP_US);
        return MouseKey_SPI_ReadDataWord(value);
    }

    if (cmd == 1U)
    {
        if (MouseKey_SPI_SendCmd(2U) != HAL_OK)
        {
            return HAL_ERROR;
        }
        bsp_DelayUS(MOUSEKEY_CMD_DATA_GAP_US);
        return MouseKey_SPI_ReadDataWord(value);
    }

    if (cmd == 2U)
    {
        if (MouseKey_SPI_SendCmd(0U) != HAL_OK)
        {
            return HAL_ERROR;
        }
        bsp_DelayUS(MOUSEKEY_CMD_DATA_GAP_US);
        return MouseKey_SPI_ReadDataWord(value);
    }

    return HAL_ERROR;
}

static void MouseKey_UpdateMouseValidity(uint32_t reg1_data)
{
    uint8_t x_raw = (uint8_t)((reg1_data >> 16) & 0xFFU);
    uint8_t y_raw = (uint8_t)((reg1_data >> 8) & 0xFFU);
    uint8_t wheel_raw = (uint8_t)(reg1_data & 0xFFU);
    uint8_t button_raw = (uint8_t)((reg1_data >> 24) & 0xFFU);
    uint8_t is_wheel_active = (uint8_t)(wheel_raw != g_last_wheel_val);
    uint8_t is_idle = (uint8_t)((x_raw == 0U) && (y_raw == 0U) && (wheel_raw == 0U) && (button_raw == 0U));
    uint8_t has_move = (uint8_t)((x_raw != 0U) || (y_raw != 0U));

    if ((!is_idle) && (reg1_data != g_last_mouse_data) && ((has_move != 0U) || (is_wheel_active != 0U)))
    {
        g_mouse_data_valid = 1U;
        g_last_mouse_data = reg1_data;
        g_last_ctrl_data = button_raw;
        g_last_wheel_val = wheel_raw;
    }
    else if ((g_last_ctrl_data == button_raw) && (button_raw != 0U))
    {
        g_mouse_data_valid = 0U;
        g_last_ctrl_data = button_raw;
    }
    else if (is_wheel_active != 0U)
    {
        g_mouse_data_valid = 1U;
        g_last_mouse_data = reg1_data;
        g_last_ctrl_data = button_raw;
        g_last_wheel_val = wheel_raw;
    }
    else
    {
        g_mouse_data_valid = 0U;
    }
}

static void MouseKey_ApplyMouseMove(uint32_t reg1_data)
{
    int8_t x_offset = (int8_t)((reg1_data >> 16) & 0xFFU);
    int8_t y_offset = (int8_t)((reg1_data >> 8) & 0xFFU);
    int32_t move_x;
    int32_t move_y;

    if (abs((int)x_offset) < (MOUSEKEY_OFFSET_THRESHOLD * 2))
    {
        x_offset = 0;
    }
    if (abs((int)y_offset) < (MOUSEKEY_OFFSET_THRESHOLD * 2))
    {
        y_offset = 0;
    }

    if ((x_offset == 0) && (y_offset == 0))
    {
        return;
    }

    y_offset = (int8_t)(-y_offset);

    move_x = ((int32_t)x_offset * (int32_t)MOUSEKEY_SCALE_X_COEFF * (int32_t)MOUSEKEY_SENSITIVITY_PERCENT) / 10000;
    move_y = ((int32_t)y_offset * (int32_t)MOUSEKEY_SCALE_Y_COEFF * (int32_t)MOUSEKEY_SENSITIVITY_PERCENT) / 10000;

    if (abs((int)move_x) < 1)
    {
        move_x = 0;
    }
    if (abs((int)move_y) < 1)
    {
        move_y = 0;
    }

    if (move_x != 0)
    {
        g_mousekey_x = (int16_t)((int32_t)g_mousekey_x + move_x);
    }
    if (move_y != 0)
    {
        g_mousekey_y = (int16_t)((int32_t)g_mousekey_y + move_y);
    }

    if (g_mousekey_x < 0)
    {
        g_mousekey_x = 0;
    }
    else if (g_mousekey_x >= MOUSEKEY_SCREEN_WIDTH)
    {
        g_mousekey_x = (int16_t)(MOUSEKEY_SCREEN_WIDTH - 1);
    }

    if (g_mousekey_y < 0)
    {
        g_mousekey_y = 0;
    }
    else if (g_mousekey_y >= MOUSEKEY_SCREEN_HEIGHT)
    {
        g_mousekey_y = (int16_t)(MOUSEKEY_SCREEN_HEIGHT - 1);
    }
}

static uint8_t MouseKey_DecodeWheel(uint32_t reg1_data)
{
    uint8_t wheel = (uint8_t)(reg1_data & 0xFFU);

    if ((wheel == 0x01U) || (wheel == 0xFFU))
    {
        return wheel;
    }

    return 0U;
}

static void MouseKey_DecodeKeyboard(uint32_t reg0,
                                    uint32_t reg1,
                                    uint32_t reg2,
                                    MouseKeyKeyboardState_t *state)
{
    uint8_t valid;
    uint8_t type;

    if (state == NULL)
    {
        return;
    }

    memset(state, 0, sizeof(*state));

    valid = (uint8_t)(reg0 & MOUSEKEY_REG_VALID_MASK);
    type = (uint8_t)((reg0 >> MOUSEKEY_REG_TYPE_SHIFT) & 0xFFU);
    if ((valid != MOUSEKEY_REG_KEYBOARD_VALID) || (type != MOUSEKEY_REG_KEYBOARD_TYPE))
    {
        return;
    }

    state->modifier = (uint8_t)((reg1 >> 24) & 0xFFU);
    state->key_codes[0] = (uint8_t)((reg1 >> 8) & 0xFFU);
    state->key_codes[1] = (uint8_t)(reg1 & 0xFFU);
    state->key_codes[2] = (uint8_t)((reg2 >> 24) & 0xFFU);
    state->key_codes[3] = (uint8_t)((reg2 >> 16) & 0xFFU);
    state->key_codes[4] = (uint8_t)((reg2 >> 8) & 0xFFU);
    state->key_codes[5] = (uint8_t)(reg2 & 0xFFU);
}

static void MouseKey_UpdatePacketFromRegs(MouseKeyPacket_t *packet, const uint32_t regs[3])
{
    MouseKeyKeyboardState_t keyboard;

    if ((packet == NULL) || (regs == NULL))
    {
        return;
    }

    packet->header = 0x55U;
    packet->mouse_ctrl = 0U;
    packet->mouse_wheel = 0U;
    packet->x_coord = g_mousekey_x;
    packet->y_coord = g_mousekey_y;
    packet->key_mod = 0U;
    memset(packet->key_code, 0, sizeof(packet->key_code));

    if (regs[0] == MOUSEKEY_REG_MOUSE_FRAME)
    {
        packet->mouse_ctrl = (uint8_t)((regs[1] >> 24) & 0xFFU);
        packet->mouse_wheel = MouseKey_DecodeWheel(regs[1]);
        MouseKey_UpdateMouseValidity(regs[1]);
        if (g_mouse_data_valid != 0U)
        {
            MouseKey_ApplyMouseMove(regs[1]);
        }
        packet->x_coord = g_mousekey_x;
        packet->y_coord = g_mousekey_y;
    }

    MouseKey_DecodeKeyboard(regs[0], regs[1], regs[2], &keyboard);
    packet->key_mod = keyboard.modifier;
    memcpy(packet->key_code, keyboard.key_codes, sizeof(packet->key_code));
}

static uint8_t MouseKey_USB_CanTransmit(void)
{
    USBD_CDC_HandleTypeDef *cdc;

    if (hUsbDeviceHS.dev_state != USBD_STATE_CONFIGURED)
    {
        return 0U;
    }

    cdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceHS.pClassData;
    if (cdc == NULL)
    {
        return 0U;
    }

    return (uint8_t)(cdc->TxState == 0U);
}

static void MouseKey_PackUsbFrame(const MouseKeyPacket_t *packet, uint8_t *buf)
{
    if ((packet == NULL) || (buf == NULL))
    {
        return;
    }

    buf[0] = packet->header;
    buf[1] = packet->mouse_ctrl;
    buf[2] = packet->mouse_wheel;
    buf[3] = (uint8_t)(packet->x_coord & 0xFF);
    buf[4] = (uint8_t)((packet->x_coord >> 8) & 0xFF);
    buf[5] = (uint8_t)(packet->y_coord & 0xFF);
    buf[6] = (uint8_t)((packet->y_coord >> 8) & 0xFF);
    buf[7] = packet->key_mod;
    memcpy(&buf[8], packet->key_code, 6U);
}

static void MouseKey_USB_TrySendPacket(const MouseKeyPacket_t *packet)
{
    if ((packet == NULL) || (MouseKey_USB_CanTransmit() == 0U))
    {
        return;
    }

    MouseKey_PackUsbFrame(packet, g_mousekey_tx_buf);
    (void)CDC_Transmit_FS(g_mousekey_tx_buf, MOUSEKEY_USB_PACKET_LEN);
}

void mousekeyDefaultTask(void const * argument)
{
    uint32_t regs[3] = {0U, 0U, 0U};

    (void)argument;

    MouseKey_SPI_ConfigSlow();
    bsp_InitDWT();
    MouseKey_CS_AllHigh();

    for (;;)
    {
        if (MouseKey_IsTxMutedByConfigFlow() != 0U)
        {
            osDelay(5);
            continue;
        }

        g_mousekey_debug_loop_count++;

        if ((MouseKey_SPI_ReadRegister(0U, &regs[0]) == HAL_OK) &&
            (MouseKey_SPI_ReadRegister(1U, &regs[1]) == HAL_OK) &&
            (MouseKey_SPI_ReadRegister(2U, &regs[2]) == HAL_OK))
        {
            g_mousekey_debug_regs[0] = regs[0];
            g_mousekey_debug_regs[1] = regs[1];
            g_mousekey_debug_regs[2] = regs[2];
            g_mousekey_debug_ok_count++;
            g_mousekey_debug_last_status = 1U;
            MouseKey_UpdatePacketFromRegs(&g_mousekey_packet, regs);
            MouseKey_USB_TrySendPacket(&g_mousekey_packet);
        }
        else
        {
            g_mousekey_debug_fail_count++;
            g_mousekey_debug_last_status = 0U;
        }

        osDelay(1);
    }
}
