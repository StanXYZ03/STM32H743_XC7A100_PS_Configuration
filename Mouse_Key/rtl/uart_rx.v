/*
 * File: uart_rx.v
 * File Created: Wednesday, 4th March 2026 3:32:52 pm
 * Author: 赵祥宇
 * -----
 * Last Modified: Thursday, 5th March 2026 2:32:29 pm
 * Modified By: 赵祥宇
 * -----
 * Copyright (c) 2026 北京革新创展科技有限公司
 * 功能：UART接收模块，50MHz时钟，115200波特率，8位数据位，无校验位，1位停止位
 */

module uart_rx (
    input  wire clk,          // 系统时钟，50MHz
    input  wire rst_n,        // 低电平有效复位
    input  wire rx,           // UART接收数据线
    output reg  [7:0] rx_byte, // 接收的单字节数据
    output reg        rx_byte_valid // 接收字节有效标志（高电平有效）
);

    // 波特率分频参数计算：50MHz / 115200 ≈ 434
    parameter CLK_FREQ = 50000000;    // 时钟频率
    parameter BAUD_RATE = 115200;     // 波特率
    // 波特率分频系数：每个比特需要的时钟周期数
    parameter BAUD_DIV = CLK_FREQ/BAUD_RATE; // 50MHz/115200 ≈ 434

    reg [15:0] baud_cnt;    // 波特率计数器（0~BAUD_DIV-1）
    reg [3:0]  bit_cnt;     // 数据位计数器（0~7）
    reg [2:0]  state;       // 状态机寄存器
    reg        rx_sync;     // RX信号同步寄存器（消除亚稳态）

    // 状态机定义
    localparam IDLE  = 3'd0;   // 空闲状态
    localparam START = 3'd1;   // 起始位检测状态
    localparam DATA  = 3'd2;   // 数据位接收状态
    localparam STOP  = 3'd3;   // 停止位检测状态

    // RX信号同步（两级寄存器，消除亚稳态）
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            rx_sync <= 1'b1;   // 空闲时RX为高电平
        end else begin
            rx_sync <= rx;
        end
    end

    // UART接收状态机
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            state <= IDLE;
            baud_cnt <= 0;
            bit_cnt <= 0;
            rx_byte <= 0;
            rx_byte_valid <= 0;
        end else begin
            case (state)
                IDLE: begin
                    rx_byte_valid <= 0;  // 空闲时有效标志置0
                    if (rx_sync == 0) begin // 检测到起始位（低电平）
                        state <= START;
                        baud_cnt <= 0;
                    end
                end

                START: begin
                    // 等待到起始位中间位置（BAUD_DIV/2），验证起始位有效性
                    if (baud_cnt == (BAUD_DIV/2 - 1)) begin
                        if (rx_sync == 0) begin // 确认是有效起始位
                            state <= DATA;
                            baud_cnt <= 0;
                            bit_cnt <= 0;
                        end else begin // 起始位无效，返回空闲状态
                            state <= IDLE;
                        end
                    end else begin
                        baud_cnt <= baud_cnt + 1;
                    end
                end

                DATA: begin
                    // 每个比特周期采样一次数据
                    if (baud_cnt == (BAUD_DIV - 1)) begin
                        rx_byte[bit_cnt] <= rx_sync; // 采样当前数据位
                        bit_cnt <= bit_cnt + 1;
                        baud_cnt <= 0;
                        if (bit_cnt == 7) begin // 8位数据接收完成
                            state <= STOP;
                        end
                    end else begin
                        baud_cnt <= baud_cnt + 1;
                    end
                end

                STOP: begin
                    // 停止位检测（高电平）
                    if (baud_cnt == (BAUD_DIV - 1)) begin
                        rx_byte_valid <= 1'b1; // 接收完成，有效标志置1
                        state <= IDLE;         // 返回空闲状态
                        baud_cnt <= 0;
                    end else begin
                        baud_cnt <= baud_cnt + 1;
                    end
                end

                default: state <= IDLE; // 异常状态返回空闲
            endcase
        end
    end

endmodule