module spi_flash_bridge_top (
    input  wire clk_100MHz,
    inout  wire spi_io0_io,
    inout  wire spi_io1_io,
    inout  wire spi_io2_io,
    inout  wire spi_io3_io,
    output wire spi_cs_o
);

    wire [39:0] cmd_word;
    wire [39:0] rsp_word;
    wire        cmd_valid;
    wire        eos;

    wire spi_cclk;
    wire spi_io0_i;
    wire spi_io0_o;
    wire spi_io0_t;
    wire spi_io1_i;
    wire spi_io1_o;
    wire spi_io1_t;
    wire spi_io2_i;
    wire spi_io2_o;
    wire spi_io2_t;
    wire spi_io3_i;
    wire spi_io3_o;
    wire spi_io3_t;

    IOBUF u_spi_io0_iobuf (
        .I (spi_io0_o),
        .O (spi_io0_i),
        .IO(spi_io0_io),
        .T (spi_io0_t)
    );

    IOBUF u_spi_io1_iobuf (
        .I (spi_io1_o),
        .O (spi_io1_i),
        .IO(spi_io1_io),
        .T (spi_io1_t)
    );

    IOBUF u_spi_io2_iobuf (
        .I (spi_io2_o),
        .O (spi_io2_i),
        .IO(spi_io2_io),
        .T (spi_io2_t)
    );

    IOBUF u_spi_io3_iobuf (
        .I (spi_io3_o),
        .O (spi_io3_i),
        .IO(spi_io3_io),
        .T (spi_io3_t)
    );

    jtag_user_bridge #(
        .WIDTH(40)
    ) u_jtag_user_bridge (
        .clk      (clk_100MHz),
        .rst_n    (eos),
        .cmd_valid(cmd_valid),
        .cmd_word (cmd_word),
        .rsp_word (rsp_word)
    );

    spi_flash_bridge_core u_spi_flash_bridge_core (
        .clk       (clk_100MHz),
        .rst_n     (eos),
        .cmd_valid (cmd_valid),
        .cmd_word  (cmd_word),
        .rsp_word  (rsp_word),
        .spi_cclk_o(spi_cclk),
        .spi_cs_o  (spi_cs_o),
        .spi_io0_o (spi_io0_o),
        .spi_io0_t (spi_io0_t),
        .spi_io1_i (spi_io1_i),
        .spi_io1_o (spi_io1_o),
        .spi_io1_t (spi_io1_t),
        .spi_io2_i (spi_io2_i),
        .spi_io2_o (spi_io2_o),
        .spi_io2_t (spi_io2_t),
        .spi_io3_i (spi_io3_i),
        .spi_io3_o (spi_io3_o),
        .spi_io3_t (spi_io3_t)
    );

    STARTUPE2 u_startupe2 (
        .CFGCLK    (),
        .CFGMCLK   (),
        .EOS       (eos),
        .PREQ      (),
        .CLK       (1'b0),
        .GSR       (1'b0),
        .GTS       (1'b0),
        .KEYCLEARB (1'b1),
        .PACK      (1'b0),
        .USRCCLKO  (spi_cclk),
        .USRCCLKTS (1'b0),
        .USRDONEO  (1'b1),
        .USRDONETS (1'b1)
    );

endmodule
