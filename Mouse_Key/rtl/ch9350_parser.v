module ch9350_parser (
    input  wire        clk,
    input  wire        rst_n,
    input  wire [7:0]  uart_byte,
    input  wire        uart_byte_valid,
    output wire [95:0] kb_mouse_data,
    output wire        kb_mouse_valid
);

    localparam HEAD1          = 8'h57;
    localparam HEAD2          = 8'hAB;
    localparam OP_KEY         = 8'h01;
    localparam OP_MOUSE       = 8'h02;
    localparam KEY_DATA_LEN   = 8'd8;
    localparam MOUSE_DATA_LEN = 8'd4;

    localparam IDLE        = 4'd0;
    localparam HEAD1_R     = 4'd1;
    localparam HEAD2_R     = 4'd2;
    localparam OP_R_UNUSED = 4'd3;
    localparam DATA_R      = 4'd4;
    localparam DONE        = 4'd5;
    localparam ERROR_RESET = 4'd6;
    localparam DONE_HOLD   = 4'd7;

    reg [3:0]  state;
    reg [7:0]  opcode;
    reg [3:0]  data_cnt;
    reg [7:0]  data_len;
    reg [95:0] data_buffer;

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            state       <= IDLE;
            opcode      <= 8'h00;
            data_cnt    <= 4'd0;
            data_len    <= 8'd0;
            data_buffer <= 96'h0;
        end else begin
            if (uart_byte_valid) begin
                case (state)
                    IDLE: begin
                        if (uart_byte == HEAD1) begin
                            state <= HEAD1_R;
                        end
                    end

                    HEAD1_R: begin
                        if (uart_byte == HEAD2) begin
                            state <= HEAD2_R;
                        end else begin
                            state <= ERROR_RESET;
                        end
                    end

                    HEAD2_R: begin
                        opcode   <= uart_byte;
                        data_cnt <= 4'd0;
                        if (uart_byte == OP_KEY) begin
                            data_len <= KEY_DATA_LEN;
                            state    <= DATA_R;
                        end else if (uart_byte == OP_MOUSE) begin
                            data_len <= MOUSE_DATA_LEN;
                            state    <= DATA_R;
                        end else begin
                            state <= ERROR_RESET;
                        end
                    end

                    DATA_R: begin
                        data_buffer[(data_cnt + 2) * 8 +: 8] <= uart_byte;
                        data_cnt <= data_cnt + 1'b1;
                        if (data_cnt == data_len - 1'b1) begin
                            state <= DONE;
                        end
                    end

                    DONE: begin
                        if (uart_byte == HEAD1) begin
                            state <= HEAD1_R;
                        end else begin
                            state <= DONE_HOLD;
                        end
                    end

                    DONE_HOLD: begin
                        if (uart_byte == HEAD1) begin
                            state <= HEAD1_R;
                        end else begin
                            state <= IDLE;
                        end
                    end

                    ERROR_RESET: begin
                        state <= IDLE;
                    end

                    default: begin
                        state <= IDLE;
                    end
                endcase
            end else if (state == DONE_HOLD) begin
                state <= IDLE;
            end
        end
    end

    assign kb_mouse_valid = (state == DONE || state == DONE_HOLD);
    assign kb_mouse_data  = (state == DONE || state == DONE_HOLD)
        ? {data_buffer[95:16], (opcode == OP_KEY) ? 8'h00 : 8'h01, 8'h01}
        : 96'h0;

endmodule