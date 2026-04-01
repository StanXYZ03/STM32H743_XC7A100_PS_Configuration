#ifndef __FPGA_CONFIG_H
#define __FPGA_CONFIG_H

#include "stm32h7xx_hal.h"
#include "usbd_cdc_if.h"
#include "stdint.h"
#include "string.h"
#include "stdio.h"
#include "inttypes.h"
#include "sdram.h"
#include "spi.h"
#include "dma.h"
#include <stdbool.h>

#define FPGA_CCLK_PORT    GPIOE
#define FPGA_CCLK_PIN     GPIO_PIN_2
#define FPGA_DATA0_PORT   GPIOE
#define FPGA_DATA0_PIN    GPIO_PIN_6

#define JTAG_GPIO_PORT         GPIOE
#define JTAG_TCK_PIN           GPIO_PIN_2
#define JTAG_TDI_PIN           GPIO_PIN_6
#define JTAG_TDO_PIN           GPIO_PIN_5
#define JTAG_TMS_PIN           GPIO_PIN_4
#define XILINX_INST_USER1      0x02U
#define XILINX_INST_JPROGRAM   0x0B
#define XILINX_INST_CFG_IN     0x05
#define XILINX_INST_JSTART     0x0C
#define XILINX_INST_BYPASS     0x3F
#define XILINX_INST_IDCODE     0x09
#define JTAG_GPIO_CLK_ENABLE() __HAL_RCC_GPIOE_CLK_ENABLE()

#define FPGA_PROGB_PORT   GPIOE
#define FPGA_PROGB_PIN    GPIO_PIN_3
#define FPGA_INITB_PORT   GPIOC
#define FPGA_INITB_PIN    GPIO_PIN_13
#define FPGA_DONE_PORT    GPIOC
#define FPGA_DONE_PIN     GPIO_PIN_14

#define XILINX_IR_LEN            6U
#define JTAG_FREQ_DEFAULT_KHZ    800U
#define JTAG_BUFFER_SIZE         4096U

#define CMD_START_FPGA_CONFIG_BYTE1 0x12
#define CMD_START_FPGA_CONFIG_BYTE2 0x31
#define CMD_START_XSVF_EXEC_BYTE1   0x21
#define CMD_START_XSVF_EXEC_BYTE2   0x43
#define CCLK_HIGH_MIN_NS           30U
#define CCLK_LOW_MIN_NS            30U
#define DATA_SETUP_MIN_NS          20U
#define DATA_HOLD_MIN_NS           20U
#define PROGB_LOW_DELAY             2U

#define BRIDGE_FRAME_BYTES    5U
#define BRIDGE_FRAME_BITS     (BRIDGE_FRAME_BYTES * 8U)

#define BRIDGE_CMD_NOP        0x00U
#define BRIDGE_CMD_SET_CS     0x01U
#define BRIDGE_CMD_XFER8      0x02U
#define BRIDGE_CMD_SET_DIV    0x03U
#define BRIDGE_CMD_STATUS     0x04U
#define BRIDGE_CMD_STATUS_EX  0x05U
#define BRIDGE_CMD_ECHO       0x06U

#define BRIDGE_STATUS_OK      0x00U
#define BRIDGE_STATUS_BUSY    0x01U

#define BRIDGE_SPI_CLK_DIV    24U

#define SPI_FLASH_PAGE_SIZE   256U
#define SPI_FLASH_SECTOR_SIZE 4096U

#define SPI_CMD_WRITE_ENABLE  0x06
#define SPI_CMD_SECTOR_ERASE  0x20
#define SPI_CMD_PAGE_PROGRAM  0x02
#define SPI_CMD_READ_DATA     0x03
#define SPI_CMD_READ_STATUS1  0x05
#define SPI_CMD_RESET_ENABLE  0x66
#define SPI_CMD_READ_FLAG_STATUS 0x70
#define SPI_CMD_RESET_MEMORY  0x99
#define SPI_CMD_READ_JEDEC_ID 0x9F

typedef enum {
    FPGA_STATE_IDLE = 0,
    FPGA_STATE_RESET,
    FPGA_STATE_WAIT_INITB,
    FPGA_STATE_SENDING,
    FPGA_STATE_WAIT_DONE,
    FPGA_STATE_SUCCESS,
    FPGA_STATE_FAILED,
    FPGA_STATE_READY_FOR_CONFIG
} FPGA_StateTypeDef;

typedef enum {
    FPGA_MODE_JTAG = 0,
    FPGA_MODE_SLAVE_SERIAL = 1
} FPGA_ModeType;

typedef enum {
    JTAG_TLR = 0,
    JTAG_RTI,
    JTAG_SDS,
    JTAG_CDR,
    JTAG_SDR,
    JTAG_E1D,
    JTAG_PDR,
    JTAG_E2D,
    JTAG_UDR,
    JTAG_SIS,
    JTAG_CIR,
    JTAG_SIR,
    JTAG_E1I,
    JTAG_PIR,
    JTAG_E2I,
    JTAG_UIR,
    JTAG_STATE_COUNT
} JtagState;

typedef struct {
    void (*SetTMS)(bool level);
    void (*SetTCK)(bool level);
    void (*SetTDI)(bool level);
    bool (*ReadTDO)(void);
} JtagHalCallbacks;

typedef struct {
    uint8_t  buffer[JTAG_BUFFER_SIZE];
    uint32_t bit_count;
} JtagTransfer;

typedef struct {
    JtagState        current_state;
    bool             last_bit_held;
    uint8_t          last_bit;
    JtagHalCallbacks *hal;
    JtagTransfer     *xfer;
} JtagContext;

extern FPGA_StateTypeDef g_fpga_state;
extern uint8_t g_fpga_config_start;
extern uint8_t INIT_STATE;
extern uint8_t PROG_STATE;
extern uint8_t DONE_STATE;
extern uint32_t send_total;

void FPGA_Delay_NS(uint32_t ns);
HAL_StatusTypeDef Jtag_ConfigureFromSdram(uint32_t bin_size);
HAL_StatusTypeDef FPGA_Send_Bin_From_SDRAM(uint32_t bin_size);
void FPGA_Reset(void);
void FPGA_Check_Config_Cmd(uint8_t *buf, uint32_t len);

void Jtag_Init(JtagContext *jtag, JtagHalCallbacks *hal, JtagTransfer *xfer);
void Jtag_Reset(JtagContext *jtag);
void Jtag_GotoState(JtagContext *jtag, JtagState target_state);
void Jtag_RunClocks(JtagContext *jtag, uint32_t count, JtagState end_state);
void Jtag_WriteInstruction(JtagContext *jtag, uint8_t inst, JtagState end_state);
void Jtag_WriteData(JtagContext *jtag, const uint8_t *data, uint32_t bit_len, JtagState end_state);
void Jtag_ReadData(JtagContext *jtag, uint8_t *data, uint32_t bit_len, JtagState end_state);
JtagHalCallbacks *BSP_Jtag_GetHalOps(void);

void FPGA_Switch_Mode(FPGA_ModeType mode);

HAL_StatusTypeDef spi_flash_init(JtagContext *jtag);
HAL_StatusTypeDef spi_flash_erase_sector(JtagContext *jtag, uint32_t sector_addr);
HAL_StatusTypeDef spi_flash_program_page(JtagContext *jtag, uint32_t page_addr, const uint8_t *data, uint16_t len);
uint8_t spi_flash_read_byte(JtagContext *jtag, uint32_t addr);
HAL_StatusTypeDef spi_flash_program_full(uint8_t *bin_data, uint32_t bin_len);

#endif
