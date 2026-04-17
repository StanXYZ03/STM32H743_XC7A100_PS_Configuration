/*
 * File: spi_cmd_data.v
 * File Created: Tuesday, 3rd March 2026 10:02:26 am
 * Author: 赵祥宇
 * -----
 * Last Modified: Thursday, 5th March 2026 2:31:51 pm
 * Modified By: 赵祥宇
 * -----
 * Copyright (c) 2026 北京革新创展科技有限公司
 */

// 核心控制层：实例化spi_reg_buf.v，所有信号一一对应
module spi_cmd_data #(
    parameter  DATA_WIDTH      = 32,   // 数据位宽
    parameter  CMD_WIDTH       = 8,    // 命令位宽
    parameter  CHANNEL_NUMBER  = 16    // 通道数
)
(
    // 系统信号
    input               clk,
    input               rst_n,
    
    // SPI物理接口
    input               spi_cs_cmd,   // 命令片选
    input               spi_cs_data,  // 数据片选
    input               spi_scl,      // SPI时钟
    input               spi_sdi,      // MOSI
    output              spi_sdo,      // MISO

    // 连接顶层的寄存器接口：直接透传到spi_reg_buf
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
    input   [DATA_WIDTH-1:0]  write_reg_15,
    output                   reg1_sent    // reg1(通道1)发送完成脉冲，用于滚轮值读后清零
);

// --------------------------
// 1. 命令通道：8位宽，处理spi_cs_cmd
// --------------------------
(* mark_debug = "true" *) wire [CMD_WIDTH-1:0]  cmd_addr;
(* mark_debug = "true" *) wire                  cmd_valid;

spi_core_log #(
    .DATA_WIDTH(CMD_WIDTH)
) u_spi_cmd_channel (
    .clk(clk),
    .rst_n(rst_n),
    .spi_sdi(spi_sdi),
    .spi_sdo(),
    .spi_cs(spi_cs_cmd),
    .spi_scl(spi_scl),
    .din({CMD_WIDTH{1'b0}}),
    .dout(cmd_addr),
    .data_valid(cmd_valid)
);

// --------------------------
// 2. 数据通道：32位宽，处理spi_cs_data
// --------------------------
(* mark_debug = "true" *) wire [DATA_WIDTH-1:0]  rx_data;
(* mark_debug = "true" *) wire [DATA_WIDTH-1:0]  tx_data;
(* mark_debug = "true" *) wire                    data_valid;

spi_core_log #(
    .DATA_WIDTH(DATA_WIDTH)
) u_spi_data_channel (
    .clk(clk),
    .rst_n(rst_n),
    .spi_sdi(spi_sdi),
    .spi_sdo(spi_sdo),
    .spi_cs(spi_cs_data),
    .spi_scl(spi_scl),
    .din(tx_data),
    .dout(rx_data),
    .data_valid(data_valid)
);

// --------------------------
// 3. 地址锁存：在 spi_cs_cmd 上升沿后锁存（dout_reg 在 posedge spi_cs 时已更新）
// --------------------------
(* mark_debug = "true" *) reg [3:0]  curr_ch_addr;
(* mark_debug = "true" *) reg [2:0]  cs_cmd_sync;

// --------------------------
// 3b. reg1发送完成检测：数据通道传输结束时若当前通道为1，则产生reg1_sent脉冲
//     用于鼠标滚轮值(reg1[7:0])读后清零，避免重复发送相同滚轮值
// --------------------------
(* mark_debug = "true" *) reg         data_valid_d1;
always @(posedge clk or negedge rst_n) begin
    if (!rst_n)
        data_valid_d1 <= 1'b0;
    else
        data_valid_d1 <= data_valid;
end
assign reg1_sent = data_valid_d1 && !data_valid && (curr_ch_addr == 4'd1);

always @(posedge clk or negedge rst_n)
begin
    if (!rst_n) begin
        curr_ch_addr <= 4'd0;
        cs_cmd_sync  <= 3'b111;   // cs idle = 1
    end else begin
        cs_cmd_sync <= {cs_cmd_sync[1:0], spi_cs_cmd};
        if (cs_cmd_sync[2:1] == 2'b01)
            curr_ch_addr <= cmd_addr[3:0];
    end
end

// --------------------------
// 4. 完美实例化spi_reg_buf.v，所有信号一一对应
// --------------------------
spi_reg_buf #(
    .DATA_WIDTH(DATA_WIDTH),
    .CHANNEL_NUMBER(CHANNEL_NUMBER)
) u_spi_reg_buf (
    .clk(clk),
    .rst_n(rst_n),
    
    // 写接口：连接数据通道
    .wr_en(data_valid),
    .wr_addr(curr_ch_addr),
    .wr_data(rx_data),
    
    // 读接口：连接数据通道
    .rd_addr(curr_ch_addr),
    .rd_data(tx_data),
    
    // 对外接口：直接透传到顶层
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
