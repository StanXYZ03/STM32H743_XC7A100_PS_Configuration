module gpio_probe_top (
    input  wire clk,
    output reg  probe_led = 1'b0
);

    localparam integer TOGGLE_COUNT = 50_000_000;

    reg [25:0] counter = 26'd0;

    always @(posedge clk) begin
        if (counter == TOGGLE_COUNT - 1) begin
            counter   <= 26'd0;
            probe_led <= ~probe_led;
        end else begin
            counter <= counter + 1'b1;
        end
    end

endmodule
