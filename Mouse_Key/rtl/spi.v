/*
 * File: spi.v
 * File Created: Tuesday, 3rd March 2026 9:40:17 am
 * Author: 赵祥宇
 * -----
 * Last Modified: Thursday, 5th March 2026 2:32:04 pm
 * Modified By: 赵祥宇
 * -----
 * Copyright (c) 2026 北京革新创展科技有限公司
 */

// 简单中间层：只实例化spi_cmd_data
module spi #(
    parameter  DATA_WIDTH      = 32,
    parameter  CHANNEL_NUMBER  = 16
)
(
    input               clk,
    input               rst_n,
    input               spi_cs_cmd,
    input               spi_cs_data,
    input               spi_scl,
    input               spi_sdi,
    output              spi_sdo,

    output  [DATA_WIDTH-1:0]  read_reg_0,
    output  [DATA_WIDTH-1:0]  read_reg_1,
    output  [DATA_WIDTH-1:0]  read_reg_2,
    output  [DATA_WIDTH-1:0]  read_reg_3,
    output  [DATA_WIDTH-1:0]  read_reg_4,
    output  [DATA_WIDTH-1:0]  read_reg_5,
    output  [DATA_WIDTH-1:0]  read_reg_6,
    output  [DATA_WIDTH-1:0]  read_reg_7,
    output  [DATA_WIDTH-1:0]  read_reg_8,
    output  [DATA_WIDTH-1:0]  read_reg_9,
    output  [DATA_WIDTH-1:0]  read_reg_10,
    output  [DATA_WIDTH-1:0]  read_reg_11,
    output  [DATA_WIDTH-1:0]  read_reg_12,
    output  [DATA_WIDTH-1:0]  read_reg_13,
    output  [DATA_WIDTH-1:0]  read_reg_14,
    output  [DATA_WIDTH-1:0]  read_reg_15,
    output                   reg1_sent,   // reg1(通道1)SPI发送完成脉冲，用于滚轮值读后清零

    input   [DATA_WIDTH-1:0]  write_reg_0,
    input   [DATA_WIDTH-1:0]  write_reg_1,
    input   [DATA_WIDTH-1:0]  write_reg_2,
    input   [DATA_WIDTH-1:0]  write_reg_3,
    input   [DATA_WIDTH-1:0]  write_reg_4,
    input   [DATA_WIDTH-1:0]  write_reg_5,
    input   [DATA_WIDTH-1:0]  write_reg_6,
    input   [DATA_WIDTH-1:0]  write_reg_7,
    input   [DATA_WIDTH-1:0]  write_reg_8,
    input   [DATA_WIDTH-1:0]  write_reg_9,
    input   [DATA_WIDTH-1:0]  write_reg_10,
    input   [DATA_WIDTH-1:0]  write_reg_11,
    input   [DATA_WIDTH-1:0]  write_reg_12,
    input   [DATA_WIDTH-1:0]  write_reg_13,
    input   [DATA_WIDTH-1:0]  write_reg_14,
    input   [DATA_WIDTH-1:0]  write_reg_15
);

// 只实例化spi_cmd_data，完全符合原厂分层设计
spi_cmd_data #(
    .DATA_WIDTH(DATA_WIDTH),
    .CHANNEL_NUMBER(CHANNEL_NUMBER)
) u_spi_cmd_data (
    .clk(clk),
    .rst_n(rst_n),
    .spi_cs_cmd(spi_cs_cmd),
    .spi_cs_data(spi_cs_data),
    .spi_scl(spi_scl),
    .spi_sdi(spi_sdi),
    .spi_sdo(spi_sdo),
    .read_reg_0(read_reg_0),
    .read_reg_1(read_reg_1),
    .read_reg_2(read_reg_2),
    .read_reg_3(read_reg_3),
    .read_reg_4(read_reg_4),
    .read_reg_5(read_reg_5),
    .read_reg_6(read_reg_6),
    .read_reg_7(read_reg_7),
    .read_reg_8(read_reg_8),
    .read_reg_9(read_reg_9),
    .read_reg_10(read_reg_10),
    .read_reg_11(read_reg_11),
    .read_reg_12(read_reg_12),
    .read_reg_13(read_reg_13),
    .read_reg_14(read_reg_14),
    .read_reg_15(read_reg_15),
    .reg1_sent(reg1_sent),
    .write_reg_0(write_reg_0),
    .write_reg_1(write_reg_1),
    .write_reg_2(write_reg_2),
    .write_reg_3(write_reg_3),
    .write_reg_4(write_reg_4),
    .write_reg_5(write_reg_5),
    .write_reg_6(write_reg_6),
    .write_reg_7(write_reg_7),
    .write_reg_8(write_reg_8),
    .write_reg_9(write_reg_9),
    .write_reg_10(write_reg_10),
    .write_reg_11(write_reg_11),
    .write_reg_12(write_reg_12),
    .write_reg_13(write_reg_13),
    .write_reg_14(write_reg_14),
    .write_reg_15(write_reg_15)
);

endmodule