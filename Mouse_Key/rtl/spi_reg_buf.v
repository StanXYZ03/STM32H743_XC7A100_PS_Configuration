/*
 * File: spi_reg_buf.v
 * File Created: Tuesday, 3rd March 2026 10:02:55 am
 * Author: 赵祥宇
 * -----
 * Last Modified: Tuesday, 3rd March 2026 10:05:39 am
 * Modified By: 赵祥宇
 * -----
 * Copyright (c) 2026 北京革新创展科技有限公司
 */

/*
 * 模块名称: spi_reg_buf
 * 功能描述: 16通道32位寄存器堆，存储STM32与FPGA交互的数据
 */
module spi_reg_buf #(
    parameter  DATA_WIDTH      = 32,    // 数据位宽
    parameter  CHANNEL_NUMBER  = 16     // 通道数量
)
(
    input                           clk,
    input                           rst_n,
    
    // 写接口 (SPI控制模块 -> 寄存器堆)
    input                           wr_en,         // 写使能
    input  [3:0]                    wr_addr,       // 写地址 (0-15)
    input  [DATA_WIDTH-1:0]         wr_data,       // 写数据
    
    // 读接口 (SPI控制模块 <- 寄存器堆)
    input  [3:0]                    rd_addr,       // 读地址 (0-15)
    output reg [DATA_WIDTH-1:0]     rd_data,       // 读数据
    
    // 寄存器组对外接口 (连接顶层)
    output [DATA_WIDTH-1:0]         read_reg_0,
    output [DATA_WIDTH-1:0]         read_reg_1,
    output [DATA_WIDTH-1:0]         read_reg_2,
    output [DATA_WIDTH-1:0]         read_reg_3,
    output [DATA_WIDTH-1:0]         read_reg_4,
    output [DATA_WIDTH-1:0]         read_reg_5,
    output [DATA_WIDTH-1:0]         read_reg_6,
    output [DATA_WIDTH-1:0]         read_reg_7,
    output [DATA_WIDTH-1:0]         read_reg_8,
    output [DATA_WIDTH-1:0]         read_reg_9,
    output [DATA_WIDTH-1:0]         read_reg_10,
    output [DATA_WIDTH-1:0]         read_reg_11,
    output [DATA_WIDTH-1:0]         read_reg_12,
    output [DATA_WIDTH-1:0]         read_reg_13,
    output [DATA_WIDTH-1:0]         read_reg_14,
    output [DATA_WIDTH-1:0]         read_reg_15,
    
    input  [DATA_WIDTH-1:0]         write_reg_0,
    input  [DATA_WIDTH-1:0]         write_reg_1,
    input  [DATA_WIDTH-1:0]         write_reg_2,
    input  [DATA_WIDTH-1:0]         write_reg_3,
    input  [DATA_WIDTH-1:0]         write_reg_4,
    input  [DATA_WIDTH-1:0]         write_reg_5,
    input  [DATA_WIDTH-1:0]         write_reg_6,
    input  [DATA_WIDTH-1:0]         write_reg_7,
    input  [DATA_WIDTH-1:0]         write_reg_8,
    input  [DATA_WIDTH-1:0]         write_reg_9,
    input  [DATA_WIDTH-1:0]         write_reg_10,
    input  [DATA_WIDTH-1:0]         write_reg_11,
    input  [DATA_WIDTH-1:0]         write_reg_12,
    input  [DATA_WIDTH-1:0]         write_reg_13,
    input  [DATA_WIDTH-1:0]         write_reg_14,
    input  [DATA_WIDTH-1:0]         write_reg_15
);

// 定义16个32位寄存器数组
reg [DATA_WIDTH-1:0] reg_file [0:CHANNEL_NUMBER-1];

// 对外读接口映射 (STM32 -> FPGA 数据)
assign read_reg_0  = reg_file[0];
assign read_reg_1  = reg_file[1];
assign read_reg_2  = reg_file[2];
assign read_reg_3  = reg_file[3];
assign read_reg_4  = reg_file[4];
assign read_reg_5  = reg_file[5];
assign read_reg_6  = reg_file[6];
assign read_reg_7  = reg_file[7];
assign read_reg_8  = reg_file[8];
assign read_reg_9  = reg_file[9];
assign read_reg_10 = reg_file[10];
assign read_reg_11 = reg_file[11];
assign read_reg_12 = reg_file[12];
assign read_reg_13 = reg_file[13];
assign read_reg_14 = reg_file[14];
assign read_reg_15 = reg_file[15];

// 寄存器初始化与写操作
integer i;
always @(posedge clk or negedge rst_n) 
begin
    if (!rst_n) 
    begin
        // 复位：所有寄存器清0
        for (i = 0; i < CHANNEL_NUMBER; i = i + 1)
            reg_file[i] <= {DATA_WIDTH{1'b0}};
    end
    else 
    begin
        // 优先响应SPI写操作
        if (wr_en)
            reg_file[wr_addr] <= wr_data;
        else 
        begin
            // 回环逻辑：FPGA -> STM32 数据映射
            reg_file[0]  <= write_reg_0;
            reg_file[1]  <= write_reg_1;
            reg_file[2]  <= write_reg_2;
            reg_file[3]  <= write_reg_3;
            reg_file[4]  <= write_reg_4;
            reg_file[5]  <= write_reg_5;
            reg_file[6]  <= write_reg_6;
            reg_file[7]  <= write_reg_7;
            reg_file[8]  <= write_reg_8;
            reg_file[9]  <= write_reg_9;
            reg_file[10] <= write_reg_10;
            reg_file[11] <= write_reg_11;
            reg_file[12] <= write_reg_12;
            reg_file[13] <= write_reg_13;
            reg_file[14] <= write_reg_14;
            reg_file[15] <= write_reg_15;
        end
    end
end

// 寄存器读操作 (组合逻辑)
always @(*)
begin
    rd_data = reg_file[rd_addr];
end

endmodule