`timescale 1ns/1ps

module spi_flash_bridge_core (
    input  wire        clk,
    input  wire        rst_n,
    input  wire        cmd_valid,
    input  wire [39:0] cmd_word,
    output reg  [39:0] rsp_word,
    output wire        spi_cclk_o,
    output wire        spi_cs_o,
    output wire        spi_io0_o,
    output wire        spi_io0_t,
    input  wire        spi_io1_i,
    output wire        spi_io1_o,
    output wire        spi_io1_t,
    input  wire        spi_io2_i,
    output wire        spi_io2_o,
    output wire        spi_io2_t,
    input  wire        spi_io3_i,
    output wire        spi_io3_o,
    output wire        spi_io3_t
);

    localparam [7:0] CMD_NOP       = 8'h00;
    localparam [7:0] CMD_SET_CS    = 8'h01;
    localparam [7:0] CMD_XFER8     = 8'h02;
    localparam [7:0] CMD_SET_DIV   = 8'h03;
    localparam [7:0] CMD_STATUS    = 8'h04;
    localparam [7:0] CMD_STATUS_EX = 8'h05;
    localparam [7:0] CMD_ECHO      = 8'h06;

    localparam [7:0] ST_OK         = 8'h00;
    localparam [7:0] ST_BUSY       = 8'h01;
    localparam [7:0] ST_BAD_CMD    = 8'hEE;

    localparam [1:0] S_IDLE        = 2'd0;
    localparam [1:0] S_LOW_WAIT    = 2'd1;
    localparam [1:0] S_HIGH_WAIT   = 2'd2;

    reg [1:0]  state      = S_IDLE;
    reg [15:0] clk_div    = 16'd50;
    reg [15:0] wait_cnt   = 16'd0;
    reg [15:0] sck_toggle_count = 16'd0;
    reg [7:0]  tx_shift   = 8'h00;
    reg [7:0]  rx_shift   = 8'h00;
    reg [7:0]  rx_sampled = 8'h00;
    reg [2:0]  bit_index  = 3'd0;

    reg spi_cclk_reg = 1'b0;
    reg spi_cs_reg   = 1'b1;
    reg spi_io0_reg  = 1'b0;
    reg spi_io0_oe   = 1'b0;

    assign spi_cclk_o = spi_cclk_reg;
    assign spi_cs_o   = spi_cs_reg;
    assign spi_io0_o  = spi_io0_reg;
    assign spi_io0_t  = ~spi_io0_oe;

    assign spi_io1_o  = 1'b0;
    assign spi_io1_t  = 1'b1;
    assign spi_io2_o  = 1'b1;
    assign spi_io2_t  = 1'b0;
    assign spi_io3_o  = 1'b1;
    assign spi_io3_t  = 1'b0;

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            state            <= S_IDLE;
            clk_div          <= 16'd50;
            wait_cnt         <= 16'd0;
            sck_toggle_count <= 16'd0;
            tx_shift         <= 8'h00;
            rx_shift         <= 8'h00;
            rx_sampled       <= 8'h00;
            bit_index        <= 3'd0;
            spi_cclk_reg     <= 1'b0;
            spi_cs_reg       <= 1'b1;
            spi_io0_reg      <= 1'b0;
            spi_io0_oe       <= 1'b0;
            rsp_word         <= {ST_OK, 8'h00, 8'h00, 8'h00, 8'h00};
        end else begin
            case (state)
                S_IDLE: begin
                    spi_cclk_reg <= 1'b0;

                    if (cmd_valid) begin
                        case (cmd_word[7:0])
                            CMD_NOP: begin
                                rsp_word <= {ST_OK, 8'h00, 8'h00, 8'h00, 8'h00};
                            end

                            CMD_SET_CS: begin
                                spi_cs_reg <= cmd_word[8];
                                rsp_word   <= {ST_OK, 8'h00, 8'h00, 8'h00, {7'h00, cmd_word[8]}};
                            end

                            CMD_SET_DIV: begin
                                clk_div  <= (cmd_word[23:8] == 16'h0000) ? 16'h0001 : cmd_word[23:8];
                                rsp_word <= {ST_OK, 8'h00, 8'h00, cmd_word[23:16], cmd_word[15:8]};
                            end

                            CMD_STATUS: begin
                                rsp_word <= {ST_OK, 8'h00, 8'h00, 8'h00, {5'h00, state != S_IDLE, spi_cclk_reg, spi_cs_reg}};
                            end

                            CMD_STATUS_EX: begin
                                rsp_word <= {
                                    ST_OK,
                                    sck_toggle_count[15:8],
                                    sck_toggle_count[7:0],
                                    {3'b000, state, bit_index},
                                    {2'b00, spi_io3_i, spi_io2_i, spi_io1_i, state != S_IDLE, spi_cclk_reg, spi_cs_reg}
                                };
                            end

                            CMD_ECHO: begin
                                rsp_word <= {
                                    ST_OK,
                                    cmd_word[31:24],
                                    cmd_word[23:16],
                                    cmd_word[15:8],
                                    cmd_word[7:0]
                                };
                            end

                            CMD_XFER8: begin
                                tx_shift    <= cmd_word[15:8];
                                rx_shift    <= 8'h00;
                                rx_sampled  <= 8'h00;
                                bit_index   <= 3'd7;
                                spi_io0_reg <= cmd_word[15];
                                spi_io0_oe  <= 1'b1;
                                wait_cnt    <= clk_div;
                                state       <= S_LOW_WAIT;
                                rsp_word    <= {ST_BUSY, 8'h00, 8'h00, 8'h00, 8'h00};
                            end

                            default: begin
                                rsp_word <= {ST_BAD_CMD, 8'h00, 8'h00, 8'h00, cmd_word[7:0]};
                            end
                        endcase
                    end
                end

                S_LOW_WAIT: begin
                    if (wait_cnt != 16'd0) begin
                        wait_cnt <= wait_cnt - 16'd1;
                    end else begin
                        spi_cclk_reg     <= 1'b1;
                        sck_toggle_count <= sck_toggle_count + 16'd1;
                        rx_shift         <= {rx_shift[6:0], spi_io1_i};
                        rx_sampled       <= {rx_shift[6:0], spi_io1_i};
                        wait_cnt         <= clk_div;
                        state            <= S_HIGH_WAIT;
                    end
                end

                S_HIGH_WAIT: begin
                    if (wait_cnt != 16'd0) begin
                        wait_cnt <= wait_cnt - 16'd1;
                    end else begin
                        spi_cclk_reg     <= 1'b0;
                        sck_toggle_count <= sck_toggle_count + 16'd1;

                        if (bit_index == 3'd0) begin
                            rsp_word <= {ST_OK, 8'h00, 8'h00, 8'h00, rx_sampled};
                            state    <= S_IDLE;
                        end else begin
                            tx_shift    <= {tx_shift[6:0], 1'b0};
                            bit_index   <= bit_index - 3'd1;
                            spi_io0_reg <= tx_shift[6];
                            wait_cnt    <= clk_div;
                            state       <= S_LOW_WAIT;
                        end
                    end
                end

                default: begin
                    state <= S_IDLE;
                end
            endcase
        end
    end

endmodule
