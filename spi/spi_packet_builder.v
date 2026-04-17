module spi_packet_builder
(
    input  [3:0]  mode,
    input  [4:0]  clk_sel,
    input  [7:0]  cou,
    input  [3:0]  num_a1,
    input  [3:0]  num_a2,
    input  [3:0]  num_a3,
    input  [3:0]  num_a4,
    input  [3:0]  num_a5,
    input  [3:0]  num_a6,
    input  [3:0]  num_a7,
    input  [3:0]  num_a8,
    input  [3:0]  num_b1,
    input  [3:0]  num_b2,
    input  [3:0]  num_b3,
    input  [3:0]  num_b4,
    input  [3:0]  num_b5,
    input  [3:0]  num_b6,
    input  [3:0]  num_b7,
    input  [3:0]  num_b8,
    input  [3:0]  num_c1,
    input  [3:0]  num_c2,
    input  [3:0]  num_c3,
    input  [3:0]  num_c4,
    input  [3:0]  num_c5,
    input  [3:0]  num_c6,
    input  [3:0]  num_c7,
    input  [3:0]  num_c8,
    input  [3:0]  cnt_shift,
    input         led_serial_out,
    output [47:0] spi_data_reg
);

    wire [7:0] freq_code;

    assign freq_code = {3'b000, clk_sel};

    assign spi_data_reg = (mode == 4'd0)  ? {freq_code, 4'b0000, mode, num_c8, num_c7, num_c6, num_c5, num_c4, num_c3, num_a2, num_a1} :
                          (mode == 4'd1)  ? {freq_code, 4'b0000, mode, 4'b0000, 4'b0000, 4'b0000, 4'b0000, num_a4, num_a3, num_a2, num_a1} :
                          (mode == 4'd2)  ? {freq_code, 4'b0000, mode, 4'b0000, 4'b0000, 4'b0000, 4'b0000, num_a4, num_a3, num_a2, num_a1} :
                          (mode == 4'd3)  ? {freq_code, 4'b0000, mode, num_b8, num_b7, num_b6, num_b5, num_b4, num_b3, num_b2, num_b1} :
                          (mode == 4'd4)  ? {freq_code, 4'b0000, mode, num_a4, num_a3, num_a2, num_a1, cnt_shift, 4'b0000, 4'b0000, 4'b0000} :
                          (mode == 4'd5)  ? {freq_code, 4'b0000, mode, num_c8, num_c7, num_c6, num_c5, num_c4, num_c3, num_c2, num_c1} :
                          (mode == 4'd6)  ? {freq_code, 4'b0000, mode, 4'b0000, 4'b0000, 4'b0000, 4'b0000, 4'b0000, 4'b0000, num_a2, num_a1} :
                          (mode == 4'd7)  ? {freq_code, 4'b0000, mode, {3'b000, led_serial_out}, {3'b000, num_c8[0]}, num_a3, num_a2, num_a1, 4'b0000, 4'b0000, 4'b0000} :
                          (mode == 4'd8)  ? {freq_code, 4'b0000, mode, num_a8, num_a7, num_a6, num_a5, num_a4, num_a3, num_a2, num_a1} :
                          (mode == 4'd9)  ? {freq_code, 4'b0000, mode, num_a8, num_a7, num_a6, num_a5, num_a4, num_a3, num_a2, num_a1} :
                          (mode == 4'd10) ? {freq_code, 4'b0000, mode, num_a8, num_a7, num_a6, num_a5, num_a4, num_a3, num_a2, num_a1} :
                                             {freq_code, 4'b0000, mode, 4'b0000, 4'b0000, 4'b0000, 4'b0000, 4'b0000, 4'b0000, 4'b0000, 4'b0000};

endmodule
