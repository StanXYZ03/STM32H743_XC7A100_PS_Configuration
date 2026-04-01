# SPI Flash JTAG Bridge for XC7A100T-FGG484

This directory contains a pure-RTL bridge design for programming the FPGA configuration flash
through the FPGA after a temporary bridge bitstream is loaded into SRAM over JTAG.

## Why this design

- `JTAG to AXI Master` is not used. PG174 documents it as a Vivado Tcl/logic-analyzer debug core,
  not a public raw-JTAG transport for an external MCU.
- `AXI Quad SPI` is not required for the first bring-up. This bridge talks to the flash directly
  using `BSCANE2` and `STARTUPE2`, which keeps the protocol fully under your control.
- `STARTUPE2` is required on 7-series devices to drive the configuration flash clock on `CCLK`.

## Top-level ports

- `clk_100MHz`: board oscillator input on `W19`
- `spi_io0_io`: flash `D0 / MOSI` on `P22`
- `spi_io1_io`: flash `D1 / MISO` on `R22`
- `spi_io2_io`: flash `D2` on `P21`
- `spi_io3_io`: flash `D3` on `R21`
- `spi_cs_o`: flash `CS` on `T19`

`CCLK` is not a top-level port. It is driven through `STARTUPE2.USRCCLKO`.

## JTAG access

- Instruction: `USER1`
- Recommended MCU IR constant: `0x02`
- DR length: `40` bits
- Bit order: LSB-first, matching the common Xilinx JTAG data shifting style

Command frame format, byte by byte:

1. `opcode`
2. `arg0`
3. `arg1`
4. `arg2`
5. `arg3`

Response frame format:

1. `status`
2. `data0`
3. `data1`
4. `data2`
5. `data3`

## Implemented commands

- `0x00`: NOP
- `0x01`: SET_CS
  - `arg0[0] = cs_n`
- `0x02`: XFER8
  - `arg0 = tx_byte`
  - response `data0 = rx_byte`
- `0x03`: SET_DIV
  - `arg0 = divider low byte`
  - `arg1 = divider high byte`
- `0x04`: STATUS
  - response `data0[0] = cs_n`
  - response `data0[1] = cclk level`
  - response `data0[2] = busy`

Status values:

- `0x00`: OK
- `0x01`: BUSY
- `0xEE`: unknown opcode

## Vivado flow

1. Create a new RTL project for `xc7a100tfgg484-2`.
2. Add:
   - `rtl/jtag_user_bridge.v`
   - `rtl/spi_flash_bridge_core.v`
   - `rtl/spi_flash_bridge_top.v`
   - `xdc/xc7a100tfgg484_spi_flash_bridge.xdc`
3. Set top module to `spi_flash_bridge_top`.
4. Synthesize, implement, and generate bitstream.
5. Convert the generated bitstream to `.bin` if needed for the STM32-side bridge loader.

## About your previous block design

Your earlier BD with `AXI Quad SPI` was not complete:

- `AXI_LITE` cannot be left unconnected. The SPI controller will not do anything without a master.
- `STARTUP_IO` cannot be left unconnected when the flash clock must go to the dedicated `CCLK`.
- Exporting only `SPI_0` is not enough for configuration-flash access on 7-series devices because
  `CCLK` is separate and must go through `STARTUPE2`.

For first bring-up, use this RTL design instead of the incomplete BD.
