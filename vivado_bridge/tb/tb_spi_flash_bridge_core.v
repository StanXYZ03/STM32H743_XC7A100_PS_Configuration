`timescale 1ns/1ps

module tb_spi_flash_bridge_core;

    localparam [7:0] CMD_SET_CS    = 8'h01;
    localparam [7:0] CMD_XFER8     = 8'h02;
    localparam [7:0] CMD_SET_DIV   = 8'h03;
    localparam [7:0] CMD_STATUS_EX = 8'h05;
    localparam [7:0] ST_OK         = 8'h00;
    localparam [7:0] ST_BUSY       = 8'h01;

    reg         clk       = 1'b0;
    reg         rst_n     = 1'b0;
    reg         cmd_valid = 1'b0;
    reg [39:0]  cmd_word  = 40'h0;
    wire [39:0] rsp_word;
    wire        spi_cclk_o;
    wire        spi_cs_o;
    wire        spi_io0_o;
    wire        spi_io0_t;
    wire        spi_io1_i;
    wire        spi_io1_o;
    wire        spi_io1_t;
    wire        spi_io2_o;
    wire        spi_io2_t;
    wire        spi_io3_o;
    wire        spi_io3_t;

    reg         spi_io2_i = 1'b1;
    reg         spi_io3_i = 1'b1;
    reg [7:0]   rx_byte   = 8'h00;
    reg [7:0]   status0   = 8'h00;
    reg [7:0]   status1   = 8'h00;
    reg [7:0]   status2   = 8'h00;
    reg [7:0]   status3   = 8'h00;

    always #5 clk = ~clk;

    spi_flash_bridge_core dut (
        .clk       (clk),
        .rst_n     (rst_n),
        .cmd_valid (cmd_valid),
        .cmd_word  (cmd_word),
        .rsp_word  (rsp_word),
        .spi_cclk_o(spi_cclk_o),
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

    spi_flash_jedec_model #(
        .JEDEC_ID(24'h20BA18)
    ) flash_model (
        .cs_n (spi_cs_o),
        .sck  (spi_cclk_o),
        .mosi (spi_io0_o),
        .miso (spi_io1_i)
    );

    initial begin
        $display("Starting tb_spi_flash_bridge_core...");
        $dumpfile("tb_spi_flash_bridge_core.vcd");
        $dumpvars(0, tb_spi_flash_bridge_core);

        repeat (5) @(posedge clk);
        rst_n = 1'b1;
        repeat (5) @(posedge clk);

        send_cmd(CMD_SET_CS, 32'h00000001);
        expect_status("SET_CS high");

        send_cmd(CMD_SET_DIV, 32'h00000002);
        expect_status("SET_DIV");

        send_cmd(CMD_SET_CS, 32'h00000000);
        expect_status("SET_CS low");

        read_status_ex(status0, status1, status2, status3);
        $display("STATUS_EX d0=%02X d1=%02X d2=%02X d3=%02X", status0, status1, status2, status3);

        xfer8(8'h9F, rx_byte);
        $display("CMD 0x9F response byte = 0x%02X", rx_byte);

        xfer8(8'hFF, rx_byte);
        check_byte("JEDEC manufacturer", rx_byte, 8'h20);

        xfer8(8'hFF, rx_byte);
        check_byte("JEDEC memory type", rx_byte, 8'hBA);

        xfer8(8'hFF, rx_byte);
        check_byte("JEDEC capacity", rx_byte, 8'h18);

        send_cmd(CMD_SET_CS, 32'h00000001);
        expect_status("SET_CS high end");

        read_status_ex(status0, status1, status2, status3);
        $display("STATUS_EX end d0=%02X d1=%02X d2=%02X d3=%02X", status0, status1, status2, status3);

        if (spi_io2_o !== 1'b1 || spi_io2_t !== 1'b0 ||
            spi_io3_o !== 1'b1 || spi_io3_t !== 1'b0) begin
            $display("ERROR: IO2/IO3 are not driven high as expected.");
            $fatal(1);
        end

        $display("PASS: bridge core read JEDEC ID 20 BA 18.");
        #100;
        $finish;
    end

    task send_cmd;
        input [7:0] opcode;
        input [31:0] arg;
        begin
            @(posedge clk);
            cmd_word  <= {arg, opcode};
            cmd_valid <= 1'b1;
            @(posedge clk);
            cmd_valid <= 1'b0;
            cmd_word  <= 40'h0;
        end
    endtask

    task expect_status;
        input [8*24-1:0] tag;
        begin
            wait (rsp_word[39:32] == ST_OK);
            @(posedge clk);
            $display("%0s rsp=%02X_%02X_%02X_%02X_%02X",
                     tag,
                     rsp_word[39:32], rsp_word[31:24], rsp_word[23:16],
                     rsp_word[15:8], rsp_word[7:0]);
        end
    endtask

    task xfer8;
        input [7:0] tx_byte;
        output [7:0] rx_out;
        begin
            send_cmd(CMD_XFER8, {24'h0, tx_byte});
            wait (rsp_word[39:32] == ST_BUSY);
            wait (rsp_word[39:32] == ST_OK);
            @(posedge clk);
            rx_out = rsp_word[7:0];
            $display("XFER8 tx=%02X rx=%02X", tx_byte, rx_out);
        end
    endtask

    task read_status_ex;
        output [7:0] d0;
        output [7:0] d1;
        output [7:0] d2;
        output [7:0] d3;
        begin
            send_cmd(CMD_STATUS_EX, 32'h00000000);
            wait (rsp_word[39:32] == ST_OK);
            @(posedge clk);
            d0 = rsp_word[7:0];
            d1 = rsp_word[15:8];
            d2 = rsp_word[23:16];
            d3 = rsp_word[31:24];
        end
    endtask

    task check_byte;
        input [8*24-1:0] tag;
        input [7:0] actual;
        input [7:0] expected;
        begin
            if (actual !== expected) begin
                $display("ERROR: %0s expected 0x%02X got 0x%02X", tag, expected, actual);
                $fatal(1);
            end
        end
    endtask

endmodule
