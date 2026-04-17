/*
 * File: SPI_Communication.v
 * File Created: Tuesday, 3rd March 2026 9:33:53 am
 * Author: 赵祥宇
 * -----
 * Last Modified: Thursday, 5th March 2026 9:24:09 am
 * Modified By: 赵祥宇
 * -----
 * Copyright (c) 2026 北京革新创展科技有限公司
 */

module SPI_Communication(
input  clk,				// 系统时钟
input  rst_n,        // 复位
input  spi_sdi,      // STM32发送数据,FPGA接收
input  spi_cs_data,  // 数据片选端
input  spi_cs_cmd,   // 命令片选端
input  spi_scl,      // spi时钟线
output spi_sdo,       // STM32接收数据,FPGA发送

// 新增：从CH9350解析模块接收数据（写入SPI的write_reg）
    input  [31:0] ch9350_reg0,  // 对应write_reg_0
    input  [31:0] ch9350_reg1,  // 对应write_reg_1
    input  [31:0] ch9350_reg2,  // 对应write_reg_2
    input         ch9350_frame_valid  // 新帧有效，上升沿表示新键鼠帧到达
);

// reg1_sent：来自SPI子模块，STM32读取reg1(通道1)完成后的脉冲
(* mark_debug = "true" *) wire reg1_sent;

// wheel_cleared：读后滚轮已清零标志，保持到ch9350新帧到达(frame_valid上升沿)时复位
(* mark_debug = "true" *) reg wheel_cleared;
(* mark_debug = "true" *) reg ch9350_frame_valid_d1;

// write_reg: 存储CH9350传入的数据（FPGA内部存储）
(* mark_debug = "true" *) reg [31:0] write_reg_0;
(* mark_debug = "true" *) reg [31:0] write_reg_1;
(* mark_debug = "true" *) reg [31:0] write_reg_2;

// 其余write_reg暂不使用，初始化为0
wire [31:0] write_reg_3  = 32'h0;
wire [31:0] write_reg_4  = 32'h0;
wire [31:0] write_reg_5  = 32'h0;
wire [31:0] write_reg_6  = 32'h0;
wire [31:0] write_reg_7  = 32'h0;
wire [31:0] write_reg_8  = 32'h0;
wire [31:0] write_reg_9  = 32'h0;
wire [31:0] write_reg_10 = 32'h0;
wire [31:0] write_reg_11 = 32'h0;
wire [31:0] write_reg_12 = 32'h0;
wire [31:0] write_reg_13 = 32'h0;
wire [31:0] write_reg_14 = 32'h0;
wire [31:0] write_reg_15 = 32'h0;

// read_reg: 连接到write_reg（STM32读取时从write_reg取值）
wire [31:0] read_reg_0;
wire [31:0] read_reg_1;
wire [31:0] read_reg_2;

// 其余read_reg暂不使用，初始化为0
wire [31:0] read_reg_3  = 32'h0;
wire [31:0] read_reg_4  = 32'h0;
wire [31:0] read_reg_5  = 32'h0;
wire [31:0] read_reg_6  = 32'h0;
wire [31:0] read_reg_7  = 32'h0;
wire [31:0] read_reg_8  = 32'h0;
wire [31:0] read_reg_9  = 32'h0;
wire [31:0] read_reg_10 = 32'h0;
wire [31:0] read_reg_11 = 32'h0;
wire [31:0] read_reg_12 = 32'h0;
wire [31:0] read_reg_13 = 32'h0;
wire [31:0] read_reg_14 = 32'h0;
wire [31:0] read_reg_15 = 32'h0;

always @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
        write_reg_0 <= 32'h0;
        write_reg_1 <= 32'h0;
        write_reg_2 <= 32'h0;
        wheel_cleared <= 1'b0;
        ch9350_frame_valid_d1 <= 1'b0;
    end else begin
        // 实时更新write_reg为CH9350传入的数据
        write_reg_0 <= ch9350_reg0;
        // reg1[7:0]：仅鼠标帧(reg0[15:8]==0x01)时为滚轮，读后清零；键盘帧(0x00)时原样透传
        ch9350_frame_valid_d1 <= ch9350_frame_valid;
        if (ch9350_reg0[15:8] == 8'h01) begin  // 鼠标帧
            if (reg1_sent)
                wheel_cleared <= 1'b1;
            else if (!ch9350_frame_valid_d1 && ch9350_frame_valid)
                wheel_cleared <= 1'b0;   // 新帧到达，允许新的滚轮值
            write_reg_1 <= (reg1_sent | wheel_cleared) ? {ch9350_reg1[31:8], 8'h0} : ch9350_reg1;
        end else begin  // 键盘帧
            wheel_cleared <= 1'b0;  // 非鼠标时保持复位
            write_reg_1 <= ch9350_reg1;
        end
        write_reg_2 <= ch9350_reg2;
    end
end

spi u_spi(
.clk(clk),						// 系统时钟
.rst_n(rst_n),					// 复位
.spi_sdi(spi_sdi),			// STM32发送数据,FPGA接收
.spi_cs_data(spi_cs_data),	// 数据片选端
.spi_cs_cmd(spi_cs_cmd),	// 命令片选端
.spi_scl(spi_scl),			// spi时钟线
.spi_sdo(spi_sdo),			// STM32接收数据,FPGA发送
   
   // reg in port
.read_reg_0(read_reg_0),	// STM32 -> FPGA 通道0
.read_reg_1(read_reg_1),  // STM32 -> FPGA 通道1
.read_reg_2(read_reg_2),  // STM32 -> FPGA 通道2
.read_reg_3(read_reg_3),  // STM32 -> FPGA 通道3
.read_reg_4(read_reg_4),  // STM32 -> FPGA 通道4
.read_reg_5(read_reg_5),  // STM32 -> FPGA 通道5
.read_reg_6(read_reg_6),  // STM32 -> FPGA 通道6
.read_reg_7(read_reg_7),  // STM32 -> FPGA 通道7
.read_reg_8(read_reg_8),       // STM32 -> FPGA 通道8
.read_reg_9(read_reg_9),       // STM32 -> FPGA 通道9
.read_reg_10(read_reg_10),      // STM32 -> FPGA 通道10
.read_reg_11(read_reg_11),      // STM32 -> FPGA 通道11
.read_reg_12(read_reg_12),      // STM32 -> FPGA 通道12
.read_reg_13(read_reg_13),      // STM32 -> FPGA 通道13
.read_reg_14(read_reg_14),      // STM32 -> FPGA 通道14
.read_reg_15(read_reg_15),      // STM32 -> FPGA 通道15
.reg1_sent(reg1_sent),         // reg1发送完成脉冲，用于滚轮值读后清零
   
   // reg out port
.write_reg_0(write_reg_0),	// FPGA -> STM32 通道0
.write_reg_1(write_reg_1), // FPGA -> STM32 通道1
.write_reg_2(write_reg_2), // FPGA -> STM32 通道2
.write_reg_3(write_reg_3), // FPGA -> STM32 通道3
.write_reg_4(write_reg_4), // FPGA -> STM32 通道4
.write_reg_5(write_reg_5), // FPGA -> STM32 通道5
.write_reg_6(write_reg_6), // FPGA -> STM32 通道6
.write_reg_7(write_reg_7), // FPGA -> STM32 通道7
.write_reg_8(write_reg_8),      // FPGA -> STM32 通道8
.write_reg_9(write_reg_9),      // FPGA -> STM32 通道9
.write_reg_10(write_reg_10),     // FPGA -> STM32 通道10
.write_reg_11(write_reg_11),     // FPGA -> STM32 通道11
.write_reg_12(write_reg_12),     // FPGA -> STM32 通道12
.write_reg_13(write_reg_13),     // FPGA -> STM32 通道13
.write_reg_14(write_reg_14),     // FPGA -> STM32 通道14
.write_reg_15(write_reg_15)      // FPGA -> STM32 通道15
);
defparam
u_spi.DATA_WIDTH      = 32,
u_spi.CHANNEL_NUMBER  = 16;

endmodule 
