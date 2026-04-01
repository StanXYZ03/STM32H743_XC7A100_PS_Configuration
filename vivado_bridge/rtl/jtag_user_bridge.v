module jtag_user_bridge #(
    parameter integer WIDTH = 40
) (
    input  wire             clk,
    input  wire             rst_n,
    output reg              cmd_valid,
    output reg  [WIDTH-1:0] cmd_word,
    input  wire [WIDTH-1:0] rsp_word
);

    wire capture;
    wire drck;
    wire reset;
    wire runtest;
    wire sel;
    wire shift;
    wire tck;
    wire tdi;
    wire tms;
    wire update;
    wire tdo;

    reg [WIDTH-1:0] rx_shift_dr = {WIDTH{1'b0}};
    reg [WIDTH-1:0] tx_shift_dr = {WIDTH{1'b0}};
    reg [WIDTH-1:0] cmd_word_dr = {WIDTH{1'b0}};
    reg             cmd_toggle_dr = 1'b0;

    reg [WIDTH-1:0] rsp_sync1_dr = {WIDTH{1'b0}};
    reg [WIDTH-1:0] rsp_sync2_dr = {WIDTH{1'b0}};

    reg cmd_toggle_sync1 = 1'b0;
    reg cmd_toggle_sync2 = 1'b0;

    assign tdo = tx_shift_dr[0];

    BSCANE2 #(
        .JTAG_CHAIN(1)
    ) u_bscane2 (
        .CAPTURE(capture),
        .DRCK(drck),
        .RESET(reset),
        .RUNTEST(runtest),
        .SEL(sel),
        .SHIFT(shift),
        .TCK(tck),
        .TDI(tdi),
        .TMS(tms),
        .UPDATE(update),
        .TDO(tdo)
    );

    always @(posedge drck or posedge reset) begin
        if (reset) begin
            rx_shift_dr  <= {WIDTH{1'b0}};
            tx_shift_dr  <= {WIDTH{1'b0}};
            rsp_sync1_dr <= {WIDTH{1'b0}};
            rsp_sync2_dr <= {WIDTH{1'b0}};
        end else begin
            rsp_sync1_dr <= rsp_word;
            rsp_sync2_dr <= rsp_sync1_dr;

            if (capture && sel) begin
                rx_shift_dr <= {WIDTH{1'b0}};
                tx_shift_dr <= rsp_sync2_dr;
            end else if (shift && sel) begin
                tx_shift_dr <= {1'b0, tx_shift_dr[WIDTH-1:1]};
                rx_shift_dr <= {tdi, rx_shift_dr[WIDTH-1:1]};
            end
        end
    end

    always @(posedge update or posedge reset) begin
        if (reset) begin
            cmd_word_dr   <= {WIDTH{1'b0}};
            cmd_toggle_dr <= 1'b0;
        end else if (sel) begin
            cmd_word_dr   <= rx_shift_dr;
            cmd_toggle_dr <= ~cmd_toggle_dr;
        end
    end

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            cmd_toggle_sync1 <= 1'b0;
            cmd_toggle_sync2 <= 1'b0;
            cmd_valid        <= 1'b0;
            cmd_word         <= {WIDTH{1'b0}};
        end else begin
            cmd_toggle_sync1 <= cmd_toggle_dr;
            cmd_toggle_sync2 <= cmd_toggle_sync1;
            cmd_valid        <= 1'b0;

            if (cmd_toggle_sync2 != cmd_toggle_sync1) begin
                cmd_valid <= 1'b1;
                cmd_word  <= cmd_word_dr;
            end
        end
    end

endmodule
