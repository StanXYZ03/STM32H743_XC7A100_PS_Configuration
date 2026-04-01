module spi_flash_jedec_model #(
    parameter [23:0] JEDEC_ID = 24'h20BA18
) (
    input  wire cs_n,
    input  wire sck,
    input  wire mosi,
    output wire miso
);

    reg [7:0]  cmd_shift = 8'h00;
    reg [2:0]  cmd_bit_count = 3'd0;
    reg [23:0] reply_shift = 24'h000000;
    reg [4:0]  reply_bits_left = 5'd0;
    reg        miso_reg = 1'b0;

    assign miso = miso_reg;

    always @(posedge cs_n) begin
        cmd_shift       <= 8'h00;
        cmd_bit_count   <= 3'd0;
        reply_shift     <= 24'h000000;
        reply_bits_left <= 5'd0;
        miso_reg        <= 1'b0;
    end

    always @(posedge sck) begin
        if (!cs_n) begin
            cmd_shift <= {cmd_shift[6:0], mosi};

            if (cmd_bit_count == 3'd7) begin
                cmd_bit_count <= 3'd0;
                if ({cmd_shift[6:0], mosi} == 8'h9F) begin
                    reply_shift     <= JEDEC_ID;
                    reply_bits_left <= 5'd24;
                end
            end else begin
                cmd_bit_count <= cmd_bit_count + 3'd1;
            end
        end
    end

    always @(negedge sck) begin
        if (!cs_n) begin
            if (reply_bits_left != 5'd0) begin
                miso_reg        <= reply_shift[23];
                reply_shift     <= {reply_shift[22:0], 1'b0};
                reply_bits_left <= reply_bits_left - 5'd1;
            end else begin
                miso_reg <= 1'b0;
            end
        end
    end

endmodule
