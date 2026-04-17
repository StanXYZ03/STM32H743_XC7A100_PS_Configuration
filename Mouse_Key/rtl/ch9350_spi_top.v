module ch9350_spi_top (
    input  wire clk,
    input  wire rst_n,
    input  wire ch9350_txd,
    input  wire spi_sdi,
    input  wire spi_cs_data,
    input  wire spi_cs_cmd,
    input  wire spi_scl,
    output wire spi_sdo
);

    (* mark_debug = "true" *) wire [7:0]  uart_byte;
    (* mark_debug = "true" *) wire        uart_byte_valid;
    (* mark_debug = "true" *) wire [95:0] kb_mouse_data;
    (* mark_debug = "true" *) wire        kb_mouse_valid;

    (* mark_debug = "true" *) reg [31:0] reg_data0;
    (* mark_debug = "true" *) reg [31:0] reg_data1;
    (* mark_debug = "true" *) reg [31:0] reg_data2;

    uart_rx #(
        .CLK_FREQ(100000000),
        .BAUD_RATE(115200)
    ) u_uart_rx (
        .clk(clk),
        .rst_n(rst_n),
        .rx(ch9350_txd),
        .rx_byte(uart_byte),
        .rx_byte_valid(uart_byte_valid)
    );

    ch9350_parser u_ch9350_parser (
        .clk(clk),
        .rst_n(rst_n),
        .uart_byte(uart_byte),
        .uart_byte_valid(uart_byte_valid),
        .kb_mouse_data(kb_mouse_data),
        .kb_mouse_valid(kb_mouse_valid)
    );

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            reg_data0 <= 32'h0;
            reg_data1 <= 32'h0;
            reg_data2 <= 32'h0;
        end else if (kb_mouse_valid) begin
            reg_data0 <= {16'h0, kb_mouse_data[15:8], kb_mouse_data[7:0]};
            reg_data1 <= {kb_mouse_data[23:16], kb_mouse_data[31:24], kb_mouse_data[39:32], kb_mouse_data[47:40]};
            reg_data2 <= {kb_mouse_data[55:48], kb_mouse_data[63:56], kb_mouse_data[71:64], kb_mouse_data[79:72]};
        end
    end

    SPI_Communication u_spi_comm (
        .clk(clk),
        .rst_n(rst_n),
        .spi_sdi(spi_sdi),
        .spi_cs_data(spi_cs_data),
        .spi_cs_cmd(spi_cs_cmd),
        .spi_scl(spi_scl),
        .spi_sdo(spi_sdo),
        .ch9350_reg0(reg_data0),
        .ch9350_reg1(reg_data1),
        .ch9350_reg2(reg_data2),
        .ch9350_frame_valid(kb_mouse_valid)
    );

endmodule
