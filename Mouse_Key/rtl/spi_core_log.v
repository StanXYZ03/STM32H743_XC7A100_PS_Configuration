/*
 * File: spi_core_log.v
 * File Created: Tuesday, 3rd March 2026 9:33:57 am
 * Author: 赵祥宇
 * -----
 * Last Modified: Thursday, 5th March 2026 2:31:30 pm
 * Modified By: 赵祥宇
 * -----
 * Copyright (c) 2026 北京革新创展科技有限公司
 */

module  spi_core_log #(
parameter  DATA_WIDTH  = 8                // 数据宽度
)
(
input                       clk,        // 时钟信号
input                       rst_n,      // 复位信号（低有效）
input                       spi_sdi,    // SPI 输入  MOSI
output                      spi_sdo,    // SPI 输出  MISO
input                       spi_cs,     // 通道片选信号（低有效）
input                       spi_scl,    // SPI 时钟信号 SCLK
input   [DATA_WIDTH-1:0]    din,        // FPGA -> STM32 数据
output  [DATA_WIDTH-1:0]    dout,       // 通道数据输出
output                      data_valid  // 通道数据有效信号, 高电平有效
);

// -------------------------- 寄存器声明 --------------------------
reg   [DATA_WIDTH-1:0]   Shift_Reg_Din;   // 输入移位寄存器
reg   [DATA_WIDTH-1:0]   Shift_Reg_DOUT;  // 输出移位寄存器
reg                      data_done_reg;   // 数据有效寄存器
reg   [DATA_WIDTH-1:0]   dout_reg;        // 数据输出寄存器

reg   [2:0]   spi_cs_reg;	// spi_cs同步寄存器
reg   [1:0]   spi_scl_reg;	// spi_scl同步寄存器
reg           spi_cs_neg_flag; // spi_cs下降沿标志
reg           spi_cs_pos_flag; // spi_cs上升沿标志
reg           spi_scl_pos_flag;// spi_scl上升沿标志
reg           spi_scl_neg_flag;// spi_scl下降沿标志

// -------------------------- 1. 同步检测SPI信号边沿（clk同步，消除异步问题） --------------------------
always @(posedge clk or negedge rst_n) 
begin 
  if (!rst_n) begin
    spi_cs_reg <= 3'b000;
    spi_scl_reg <= 2'b00;
    spi_cs_neg_flag <= 1'b0;
    spi_cs_pos_flag <= 1'b0;
    spi_scl_pos_flag <= 1'b0;
    spi_scl_neg_flag <= 1'b0;
  end else begin
    // 同步SPI信号到clk域
    spi_cs_reg <= {spi_cs_reg[1:0], spi_cs};
    spi_scl_reg <= {spi_scl_reg[0], spi_scl};
    
    // 检测边沿标志（仅在clk域生成，无语法错误）
    spi_cs_neg_flag <= (spi_cs_reg == 3'b100) ? 1'b1 : 1'b0; // cs下降沿
    spi_cs_pos_flag <= (spi_cs_reg == 3'b011) ? 1'b1 : 1'b0; // cs上升沿
    spi_scl_pos_flag <= (spi_scl_reg == 2'b01) ? 1'b1 : 1'b0; // scl上升沿
    spi_scl_neg_flag <= (spi_scl_reg == 2'b10) ? 1'b1 : 1'b0; // scl下降沿
  end
end 

// -------------------------- 2. 输入移位寄存器（SPI接收：SCL上升沿采样） --------------------------
always @(posedge clk or negedge rst_n) 
begin 
  if (!rst_n) begin
    Shift_Reg_Din <= {DATA_WIDTH{1'b0}};
  end else if (!spi_cs && spi_scl_pos_flag) begin // 片选有效+SCL上升沿：移位接收
    Shift_Reg_Din <= {Shift_Reg_Din[DATA_WIDTH-2:0], spi_sdi};
  end else begin
    Shift_Reg_Din <= Shift_Reg_Din; // 显式保持，消除锁存器
  end
end 

// -------------------------- 3. 接收数据锁存（CS上升沿锁存） --------------------------
always @(posedge clk or negedge rst_n) 
begin 
  if (!rst_n) begin
    dout_reg <= {DATA_WIDTH{1'b0}};
  end else if (spi_cs_pos_flag) begin // CS上升沿：锁存接收数据
    dout_reg <= Shift_Reg_Din;
  end else begin
    dout_reg <= dout_reg; // 显式保持
  end
end 
assign dout = dout_reg;

// -------------------------- 4. 数据有效信号生成 --------------------------
always @(posedge clk or negedge rst_n) 
begin 
  if (!rst_n) begin
    data_done_reg <= 1'b0;
  end else if (spi_cs_pos_flag) begin // CS上升沿：数据有效置0
    data_done_reg <= 1'b0;
  end else if (spi_cs_neg_flag) begin // CS下降沿：数据有效置1
    data_done_reg <= 1'b1;
  end else begin
    data_done_reg <= data_done_reg; // 显式保持
  end
end 
assign data_valid = data_done_reg;

// -------------------------- 5. 输出移位寄存器（SPI发送：SCL下降沿移位） --------------------------
always @(posedge clk or negedge rst_n) 
begin 
  if (!rst_n) begin
    Shift_Reg_DOUT <= {DATA_WIDTH{1'b0}};
  end else if (spi_cs_neg_flag) begin // CS下降沿：加载待发送数据
    Shift_Reg_DOUT <= din;
  end else if (!spi_cs && spi_scl_neg_flag) begin // 片选有效+SCL下降沿：移位发送
    Shift_Reg_DOUT <= {Shift_Reg_DOUT[DATA_WIDTH-2:0], 1'b0};
  end else begin
    Shift_Reg_DOUT <= Shift_Reg_DOUT; // 显式保持
  end
end 

// -------------------------- 6. SPI MISO输出 --------------------------
assign spi_sdo = (!spi_cs) ? Shift_Reg_DOUT[DATA_WIDTH-1] : 1'b1;

endmodule