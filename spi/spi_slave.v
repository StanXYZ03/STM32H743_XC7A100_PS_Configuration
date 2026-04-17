module spi_slave #(
    parameter DATALEN = 8,
    parameter DATANUM = 6
)(
    input  wire                         sys_clk,
    input  wire                         rst_n,
    // SPI从机接口 (时钟和CS来自MCU)
    input  wire                         spi_clk,
    input  wire                         spi_cs_n,
    output wire                         spi_miso,
    input  wire                         spi_mosi,
    // 数据接口
    input  wire [DATALEN*DATANUM-1:0]   tx_data,
    output reg  [DATALEN*DATANUM-1:0]   rx_data,
    output reg                          frame_done
);

    // 将外部SPI信号同步到sys_clk域
    reg [2:0] spi_clk_sync;
    reg [2:0] spi_cs_sync;
    reg [2:0] spi_mosi_sync;

    always @(posedge sys_clk or negedge rst_n) begin
        if (!rst_n) begin
            spi_clk_sync  <= 3'b000;
            spi_cs_sync   <= 3'b111;
            spi_mosi_sync <= 3'b000;
        end else begin
            spi_clk_sync  <= {spi_clk_sync[1:0], spi_clk};
            spi_cs_sync   <= {spi_cs_sync[1:0], spi_cs_n};
            spi_mosi_sync <= {spi_mosi_sync[1:0], spi_mosi};
        end
    end

    wire spi_clk_rise = spi_clk_sync[1] & ~spi_clk_sync[2];
    wire spi_clk_fall = ~spi_clk_sync[1] & spi_clk_sync[2];
    wire spi_cs_active = ~spi_cs_sync[2];
    wire spi_cs_fall = ~spi_cs_sync[1] & spi_cs_sync[2];
    wire spi_cs_rise = spi_cs_sync[1] & ~spi_cs_sync[2];
    wire mosi_in = spi_mosi_sync[2];

    // 移位寄存器
    reg [DATALEN*DATANUM-1:0] shift_tx;
    reg [DATALEN*DATANUM-1:0] shift_rx;
    reg [5:0] bit_cnt;

    // MISO输出：固定驱动移位寄存器最高位，避免output端口三态综合不确定
    assign spi_miso = shift_tx[DATALEN*DATANUM-1];

    always @(posedge sys_clk or negedge rst_n) begin
        if (!rst_n) begin
            shift_tx   <= 0;
            shift_rx   <= 0;
            bit_cnt    <= 0;
            rx_data    <= 0;
            frame_done <= 0;
        end else begin
            frame_done <= 0;

            if (spi_cs_fall) begin
                shift_tx <= tx_data;
                shift_rx <= 0;
                bit_cnt  <= 0;
            end
            else if (spi_cs_active) begin
                if (spi_clk_rise) begin
                    shift_rx <= {shift_rx[DATALEN*DATANUM-2:0], mosi_in};
                    bit_cnt  <= bit_cnt + 1;
                end
                if (spi_clk_fall) begin
                    shift_tx <= {shift_tx[DATALEN*DATANUM-2:0], 1'b0};
                end
            end

            if (spi_cs_rise) begin
                if (bit_cnt == DATALEN * DATANUM) begin
                    rx_data    <= shift_rx;
                    frame_done <= 1;
                end
            end
        end
    end

endmodule
