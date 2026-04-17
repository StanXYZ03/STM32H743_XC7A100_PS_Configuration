module STM32_Lattice_Xilinx (
    input  wire stm32_spi_cs_cmd,
    input  wire stm32_spi_cs_data,
    input  wire stm32_spi_clk,
    input  wire stm32_spi_mosi,
    output wire stm32_spi_miso,

    output wire xilinx_spi_cs_cmd,
    output wire xilinx_spi_cs_data,
    output wire xilinx_spi_clk,
    output wire xilinx_spi_mosi,
    input  wire xilinx_spi_miso
);

assign xilinx_spi_cs_cmd = stm32_spi_cs_cmd;
assign xilinx_spi_mosi = stm32_spi_mosi;
assign stm32_spi_miso  = xilinx_spi_miso;
assign xilinx_spi_clk  = stm32_spi_clk;
assign xilinx_spi_cs_data = stm32_spi_cs_data;

//assign stm32_spi_miso  = xilinx_spi_cs_data;

endmodule
