/*
 * File: tb_ch9350_spi_top.v
 * File Created: Wednesday, 4th March 2026 4:39:57 pm
 * Author: 开发者名称
 * -----
 * Last Modified: Thursday, 5th March 2026 9:52:44 am
 * Modified By: 开发者名称
 * -----
 * Copyright (c) 2026 公司/组织名称
 */

`timescale 1ns/1ps

// 完整的ch9350_spi_top模块测试平台（无内部信号绑定）
module tb_ch9350_spi_top;

    // ====================== 参数定义 ======================
    parameter CLK_FREQ     = 50_000_000;  // 50MHz系统时钟
    parameter BAUD_RATE    = 115200;      // CH9350串口波特率
    parameter BAUD_PERIOD  = 8680.5555556;// 115200波特率对应的精确位周期（ns）
    parameter SPI_CLK_HALF = 100;         // SPI时钟半周期(ns)，对应5MHz SPI时钟
    parameter SIM_TIMEOUT  = 2_000_000_000;   // 仿真超时时间2秒（7字节@115200约608us+余量）
    parameter FRAME_DELAY  = 800_000;     // 800us帧延迟：7字节@115200≈608us + 余量

    // ====================== 顶层模块端口（仅8个合法端口） ======================
    reg         clk;          // 系统时钟
    reg         rst_n;        // 低有效复位
    reg         ch9350_txd;   // CH9350 TXD（串口输入）
    reg         spi_sdi;      // SPI输入（STM32到FPGA）
    reg         spi_cs_data;  // SPI数据片选
    reg         spi_cs_cmd;   // SPI命令片选
    reg         spi_scl;      // SPI时钟
    wire        spi_sdo;      // SPI输出（FPGA到STM32）

    // ====================== 例化顶层模块 ======================
    ch9350_spi_top u_ch9350_spi_top (
        .clk(clk),
        .rst_n(rst_n),
        .ch9350_txd(ch9350_txd),
        .spi_sdi(spi_sdi),
        .spi_cs_data(spi_cs_data),
        .spi_cs_cmd(spi_cs_cmd),
        .spi_scl(spi_scl),
        .spi_sdo(spi_sdo)
    );

    // ====================== 50MHz时钟生成 ======================
    initial begin
        clk = 1'b0;
        forever #10 clk = ~clk;  // 20ns周期（50MHz）
    end

    // ====================== 仿真超时控制 ======================
    initial begin
        #SIM_TIMEOUT;
        $display("\n[TIMEOUT] Simulation timeout at %0t ns", $time);
        $finish;
    end

    // ====================== 复位序列 ======================
    initial begin
        // 初始化所有输入信号
        rst_n = 1'b0;
        ch9350_txd = 1'b1;  // 串口空闲状态（高电平）
        spi_sdi = 1'b0;
        spi_cs_data = 1'b1; // SPI片选空闲状态（高电平）
        spi_cs_cmd = 1'b1;
        spi_scl = 1'b0;

        // 复位保持200ns
        #200;
        rst_n = 1'b1;
        $display("\n=============================================");
        $display("Reset released at %0t ns", $time);
        $display("=============================================\n");
    end

    // ====================== 任务：发送一个串口字节（115200 8N1） ======================
    task send_uart_byte;
        input [7:0] byte_data;  // 要发送的字节
        integer     bit_idx;    // 位计数器
    begin
        $display("Sending UART byte: 0x%02X at %0t ns", byte_data, $time);
        
        // 起始位（0）
        ch9350_txd = 1'b0;
        #BAUD_PERIOD;
        
        // 数据位（低位在前）
        for (bit_idx = 0; bit_idx < 8; bit_idx = bit_idx + 1) begin
            ch9350_txd = byte_data[bit_idx];
            #BAUD_PERIOD;
        end
        
        // 停止位（1）
        ch9350_txd = 1'b1;
        #BAUD_PERIOD;
        
        $display("UART byte 0x%02X sent completely at %0t ns", byte_data, $time);
    end
    endtask

// ====================== SPI发送命令（8位通道地址） ======================
    task spi_send_cmd;
        input [7:0] cmd_addr;
        integer i;
    begin
        spi_cs_cmd = 1'b0;
        #(SPI_CLK_HALF * 2);
        for (i = 7; i >= 0; i = i - 1) begin
            spi_scl = 1'b0;
            #SPI_CLK_HALF;
            spi_sdi = cmd_addr[i];
            spi_scl = 1'b1;
            #SPI_CLK_HALF;
        end
        spi_scl = 1'b0;
        #(SPI_CLK_HALF * 2);
        spi_cs_cmd = 1'b1;
        #(SPI_CLK_HALF * 2);
    end
    endtask

    // 读取32位SPI数据（模拟STM32读取FPGA寄存器）
    task spi_read_32bit_data;
        output [31:0] data_out;
        integer i;
        reg [31:0] tmp;
    begin
        #1000;  // 同步延迟，确保curr_ch_addr稳定
        tmp = 32'h0;
        spi_cs_data = 1'b0;
        #(SPI_CLK_HALF * 2);
        for (i = 31; i >= 0; i = i - 1) begin
            spi_scl = 1'b0;
            #SPI_CLK_HALF;
            tmp[i] = spi_sdo;
            spi_scl = 1'b1;
            #SPI_CLK_HALF;
        end
        spi_scl = 1'b0;
        #(SPI_CLK_HALF * 2);
        spi_cs_data = 1'b1;
        #(SPI_CLK_HALF * 2);
        data_out = tmp;
    end
    endtask

    // spi_core_log: din在posedge spi_cs时加载，需先发送命令再读取（dummy读）
    // 发送命令后读取对应通道的32位数据（dummy读用于同步）
    task spi_read_32bit;
        input [7:0] ch_addr;
        output [31:0] data_out;
        reg [31:0] dummy;
    begin
        spi_send_cmd(ch_addr);
        spi_read_32bit_data(data_out);
    end
    endtask

    // ====================== 任务：发送CH9350鼠标帧 ======================
    task send_mouse_frame;
        input [7:0] btn;    // 鼠标按键（0x01=左键点击）
        input [7:0] x;      // X轴偏移
        input [7:0] y;      // Y轴偏移
        input [7:0] wheel;  // 滚轮偏移
    begin
        $display("\n=============================================");
        $display("Start sending mouse frame at %0t ns", $time);
        $display("Mouse Frame: Btn=0x%02X, X=0x%02X, Y=0x%02X, Wheel=0x%02X", btn, x, y, wheel);
        $display("=============================================\n");

        // CH9350 State 2鼠标帧结构:
        // 0x57(帧头1) -> 0xAB(帧头2) -> 0x02(鼠标操作码) -> 按键 -> X -> Y -> 滚轮
        send_uart_byte(8'h57);  // 帧头1
        send_uart_byte(8'hAB);  // 帧头2
        send_uart_byte(8'h02);  // 鼠标操作码
        send_uart_byte(btn);    // 鼠标按键状态
        send_uart_byte(x);      // X偏移
        send_uart_byte(y);      // Y偏移
        send_uart_byte(wheel);  // 滚轮偏移

        $display("\n=============================================");
        $display("Mouse frame sent completely at %0t ns", $time);
        $display("=============================================\n");
    end
    endtask

    // ====================== 任务：发送CH9350键盘帧 ======================
task send_keyboard_frame;
    input [7:0] key_byte0;  // 修饰键（Modifier keys: Ctrl/Shift/Alt/Win等）
    input [7:0] key_byte1;  // 保留位（固定0x00）
    input [7:0] key_byte2;  // 按键1（Keycode 1）
    input [7:0] key_byte3;  // 按键2（Keycode 2）
    input [7:0] key_byte4;  // 按键3（Keycode 3）
    input [7:0] key_byte5;  // 按键4（Keycode 4）
    input [7:0] key_byte6;  // 按键5（Keycode 5）
    input [7:0] key_byte7;  // 按键6（Keycode 6）
begin
    $display("\n=============================================");
    $display("Start sending keyboard frame at %0t ns", $time);
    $display("Keyboard Frame:");
    $display("  Modifier: 0x%02X, Reserved: 0x%02X", key_byte0, key_byte1);
    $display("  Keycodes: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X",
             key_byte2, key_byte3, key_byte4, key_byte5, key_byte6, key_byte7);
    $display("=============================================\n");

    // CH9350 State 2键盘帧结构:
    // 0x57(帧头1) -> 0xAB(帧头2) -> 0x01(键盘操作码) -> 8字节键盘数据
    send_uart_byte(8'h57);  // 帧头1
    send_uart_byte(8'hAB);  // 帧头2
    send_uart_byte(8'h01);  // 键盘操作码（0x01）
    send_uart_byte(key_byte0);  // 修饰键
    send_uart_byte(key_byte1);  // 保留位（0x00）
    send_uart_byte(key_byte2);  // 按键1
    send_uart_byte(key_byte3);  // 按键2
    send_uart_byte(key_byte4);  // 按键3
    send_uart_byte(key_byte5);  // 按键4
    send_uart_byte(key_byte6);  // 按键5
    send_uart_byte(key_byte7);  // 按键6

    $display("\n=============================================");
    $display("Keyboard frame sent completely at %0t ns", $time);
    $display("=============================================\n");
end
endtask


    reg [31:0] spi_reg0, spi_reg1, spi_reg2;
    // ====================== 主测试流程 ======================
    initial begin
        // 等待复位完成
        @(posedge rst_n);
        #1000;  // 额外1us延迟保证稳定

        // 发送测试鼠标帧（注释掉，改用键盘帧测试）
        send_mouse_frame(8'h00, 8'h00, 8'h00, 8'hff);

        #300

        $display("\n[DEBUG] Parser state=%0d kb_mouse_valid=%b data[15:0]=0x%04h",
            u_ch9350_spi_top.u_ch9350_parser.state,
            u_ch9350_spi_top.u_ch9350_parser.kb_mouse_valid,
            u_ch9350_spi_top.u_ch9350_parser.kb_mouse_data[15:0]);

        $display("\n[CHECK] SPI register values after parsing(Key):");
        $display("  reg_data0 = 0x%08h", u_ch9350_spi_top.reg_data0);
        $display("  reg_data1 = 0x%08h", u_ch9350_spi_top.reg_data1);
        $display("  reg_data2 = 0x%08h", u_ch9350_spi_top.reg_data2);


        $display("\n[TEST] Simulate STM32 reading SPI data...");
        // 适配spi_core_log的load逻辑：先发送命令再读取（dummy读同步）
        spi_send_cmd(8'd0);
        spi_read_32bit_data(spi_reg0);
        spi_send_cmd(8'd1);
        spi_read_32bit_data(spi_reg0);
        $display("  Read reg0: 0x%08h", spi_reg0);
        spi_send_cmd(8'd2);
        spi_read_32bit_data(spi_reg1);
        $display("  Read reg1: 0x%08h", spi_reg1);
        spi_send_cmd(8'd0);
        spi_read_32bit_data(spi_reg2);
        $display("  Read reg2: 0x%08h", spi_reg2);

        $display("\n[TEST] Simulate STM32 reading SPI data...");
        // 适配spi_core_log的load逻辑：先发送命令再读取（dummy读同步）
        spi_send_cmd(8'd0);
        spi_read_32bit_data(spi_reg0);
        spi_send_cmd(8'd1);
        spi_read_32bit_data(spi_reg0);
        $display("  Read reg0: 0x%08h", spi_reg0);
        spi_send_cmd(8'd2);
        spi_read_32bit_data(spi_reg1);
        $display("  Read reg1: 0x%08h", spi_reg1);
        spi_send_cmd(8'd0);
        spi_read_32bit_data(spi_reg2);
        $display("  Read reg2: 0x%08h", spi_reg2);

        // 发送键盘帧：按下A键（USB键盘码0x04），无修饰键
send_keyboard_frame(
    8'h04,  // 修饰键：
    8'h00,  // 保留位：0x00
    8'h00,  // 按键1：
    8'h03,  // 按键2：无
    8'h00,  // 按键3：无
    8'h00,  // 按键4：无
    8'h00,  // 按键5：无
    8'h00   // 按键6：无
);

        #300

        $display("\n[DEBUG] Parser state=%0d kb_mouse_valid=%b data[15:0]=0x%04h",
            u_ch9350_spi_top.u_ch9350_parser.state,
            u_ch9350_spi_top.u_ch9350_parser.kb_mouse_valid,
            u_ch9350_spi_top.u_ch9350_parser.kb_mouse_data[15:0]);

        $display("\n[CHECK] SPI register values after parsing(Key):");
        $display("  reg_data0 = 0x%08h", u_ch9350_spi_top.reg_data0);
        $display("  reg_data1 = 0x%08h", u_ch9350_spi_top.reg_data1);
        $display("  reg_data2 = 0x%08h", u_ch9350_spi_top.reg_data2);


        $display("\n[TEST] Simulate STM32 reading SPI data...");
        // 适配spi_core_log的load逻辑：先发送命令再读取（dummy读同步）
        spi_send_cmd(8'd0);
        spi_read_32bit_data(spi_reg0);
        spi_send_cmd(8'd1);
        spi_read_32bit_data(spi_reg0);
        $display("  Read reg0: 0x%08h", spi_reg0);
        spi_send_cmd(8'd2);
        spi_read_32bit_data(spi_reg1);
        $display("  Read reg1: 0x%08h", spi_reg1);
        spi_send_cmd(8'd0);
        spi_read_32bit_data(spi_reg2);
        $display("  Read reg2: 0x%08h", spi_reg2);

send_keyboard_frame(
    8'h00,  // 修饰键：无（Left Shift/Ctrl/Alt/Win均未按下）
    8'h00,  // 保留位：0x00
    8'h00,  // 按键1：
    8'h00,  // 按键2：无
    8'h00,  // 按键3：无
    8'h00,  // 按键4：无
    8'h77,  // 按键5：无
    8'h00   // 按键6：无
);

        #300

        $display("\n[DEBUG] Parser state=%0d kb_mouse_valid=%b data[15:0]=0x%04h",
            u_ch9350_spi_top.u_ch9350_parser.state,
            u_ch9350_spi_top.u_ch9350_parser.kb_mouse_valid,
            u_ch9350_spi_top.u_ch9350_parser.kb_mouse_data[15:0]);

        $display("\n[CHECK] SPI register values after parsing(Key):");
        $display("  reg_data0 = 0x%08h", u_ch9350_spi_top.reg_data0);
        $display("  reg_data1 = 0x%08h", u_ch9350_spi_top.reg_data1);
        $display("  reg_data2 = 0x%08h", u_ch9350_spi_top.reg_data2);


        $display("\n[TEST] Simulate STM32 reading SPI data...");
        // 适配spi_core_log的load逻辑：先发送命令再读取（dummy读同步）
        spi_send_cmd(8'd0);
        spi_read_32bit_data(spi_reg0);
        spi_send_cmd(8'd1);
        spi_read_32bit_data(spi_reg0);
        $display("  Read reg0: 0x%08h", spi_reg0);
        spi_send_cmd(8'd2);
        spi_read_32bit_data(spi_reg1);
        $display("  Read reg1: 0x%08h", spi_reg1);
        spi_send_cmd(8'd0);
        spi_read_32bit_data(spi_reg2);
        $display("  Read reg2: 0x%08h", spi_reg2);

        send_keyboard_frame(
    8'h11,  // 修饰键：无（Left Shift/Ctrl/Alt/Win均未按下）
    8'h00,  // 保留位：0x00
    8'h22,  // 按键1：
    8'h00,  // 按键2：无
    8'h00,  // 按键3：无
    8'h00,  // 按键4：无
    8'h00,  // 按键5：无
    8'h00   // 按键6：无
);

        #300

        $display("\n[DEBUG] Parser state=%0d kb_mouse_valid=%b data[15:0]=0x%04h",
            u_ch9350_spi_top.u_ch9350_parser.state,
            u_ch9350_spi_top.u_ch9350_parser.kb_mouse_valid,
            u_ch9350_spi_top.u_ch9350_parser.kb_mouse_data[15:0]);

        $display("\n[CHECK] SPI register values after parsing(Key):");
        $display("  reg_data0 = 0x%08h", u_ch9350_spi_top.reg_data0);
        $display("  reg_data1 = 0x%08h", u_ch9350_spi_top.reg_data1);
        $display("  reg_data2 = 0x%08h", u_ch9350_spi_top.reg_data2);


        $display("\n[TEST] Simulate STM32 reading SPI data...");
        // 适配spi_core_log的load逻辑：先发送命令再读取（dummy读同步）
        spi_send_cmd(8'd0);
        spi_read_32bit_data(spi_reg0);
        spi_send_cmd(8'd1);
        spi_read_32bit_data(spi_reg0);
        $display("  Read reg0: 0x%08h", spi_reg0);
        spi_send_cmd(8'd2);
        spi_read_32bit_data(spi_reg1);
        $display("  Read reg1: 0x%08h", spi_reg1);
        spi_send_cmd(8'd0);
        spi_read_32bit_data(spi_reg2);
        $display("  Read reg2: 0x%08h", spi_reg2);

        send_keyboard_frame(
    8'h00,  // 修饰键：无（Left Shift/Ctrl/Alt/Win均未按下）
    8'h00,  // 保留位：0x00
    8'h00,  // 按键1：
    8'h33,  // 按键2：无
    8'h00,  // 按键3：无
    8'h00,  // 按键4：无
    8'h00,  // 按键5：无
    8'h00   // 按键6：无
);

        #300

        $display("\n[DEBUG] Parser state=%0d kb_mouse_valid=%b data[15:0]=0x%04h",
            u_ch9350_spi_top.u_ch9350_parser.state,
            u_ch9350_spi_top.u_ch9350_parser.kb_mouse_valid,
            u_ch9350_spi_top.u_ch9350_parser.kb_mouse_data[15:0]);

        $display("\n[CHECK] SPI register values after parsing(Key):");
        $display("  reg_data0 = 0x%08h", u_ch9350_spi_top.reg_data0);
        $display("  reg_data1 = 0x%08h", u_ch9350_spi_top.reg_data1);
        $display("  reg_data2 = 0x%08h", u_ch9350_spi_top.reg_data2);


        $display("\n[TEST] Simulate STM32 reading SPI data...");
        // 适配spi_core_log的load逻辑：先发送命令再读取（dummy读同步）
        spi_send_cmd(8'd0);
        spi_read_32bit_data(spi_reg0);
        spi_send_cmd(8'd1);
        spi_read_32bit_data(spi_reg0);
        $display("  Read reg0: 0x%08h", spi_reg0);
        spi_send_cmd(8'd2);
        spi_read_32bit_data(spi_reg1);
        $display("  Read reg1: 0x%08h", spi_reg1);
        spi_send_cmd(8'd0);
        spi_read_32bit_data(spi_reg2);
        $display("  Read reg2: 0x%08h", spi_reg2);
        // 仿真结束（超时前）
        #10000;
        $finish;
    end

endmodule