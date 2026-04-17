// Verilog netlist produced by program LSE :  version Diamond (64-bit) 3.14.0.75.2
// Netlist written on Thu Apr 16 15:41:24 2026
//
// Verilog Description of module STM32_Lattice_Xilinx
//

module STM32_Lattice_Xilinx (stm32_spi_cs_cmd, stm32_spi_cs_data, stm32_spi_clk, 
            stm32_spi_mosi, stm32_spi_miso, xilinx_spi_cs_cmd, xilinx_spi_cs_data, 
            xilinx_spi_clk, xilinx_spi_mosi, xilinx_spi_miso);   // d:/cubemx/stm32h743_xc7a100_ps_configuration/stm32_lattice_xilinx.v(1[8:28])
    input stm32_spi_cs_cmd;   // d:/cubemx/stm32h743_xc7a100_ps_configuration/stm32_lattice_xilinx.v(2[17:33])
    input stm32_spi_cs_data;   // d:/cubemx/stm32h743_xc7a100_ps_configuration/stm32_lattice_xilinx.v(3[17:34])
    input stm32_spi_clk;   // d:/cubemx/stm32h743_xc7a100_ps_configuration/stm32_lattice_xilinx.v(4[17:30])
    input stm32_spi_mosi;   // d:/cubemx/stm32h743_xc7a100_ps_configuration/stm32_lattice_xilinx.v(5[17:31])
    output stm32_spi_miso;   // d:/cubemx/stm32h743_xc7a100_ps_configuration/stm32_lattice_xilinx.v(6[17:31])
    output xilinx_spi_cs_cmd;   // d:/cubemx/stm32h743_xc7a100_ps_configuration/stm32_lattice_xilinx.v(8[17:34])
    output xilinx_spi_cs_data;   // d:/cubemx/stm32h743_xc7a100_ps_configuration/stm32_lattice_xilinx.v(9[17:35])
    output xilinx_spi_clk;   // d:/cubemx/stm32h743_xc7a100_ps_configuration/stm32_lattice_xilinx.v(10[17:31])
    output xilinx_spi_mosi;   // d:/cubemx/stm32h743_xc7a100_ps_configuration/stm32_lattice_xilinx.v(11[17:32])
    input xilinx_spi_miso;   // d:/cubemx/stm32h743_xc7a100_ps_configuration/stm32_lattice_xilinx.v(12[17:32])
    
    
    wire xilinx_spi_cs_cmd_c_c, xilinx_spi_cs_data_c_c, xilinx_spi_clk_c_c, 
        xilinx_spi_mosi_c_c, stm32_spi_miso_c_c, GND_net, VCC_net;
    
    VLO i23 (.Z(GND_net));
    OB stm32_spi_miso_pad (.I(stm32_spi_miso_c_c), .O(stm32_spi_miso));   // d:/cubemx/stm32h743_xc7a100_ps_configuration/stm32_lattice_xilinx.v(6[17:31])
    OB xilinx_spi_cs_cmd_pad (.I(xilinx_spi_cs_cmd_c_c), .O(xilinx_spi_cs_cmd));   // d:/cubemx/stm32h743_xc7a100_ps_configuration/stm32_lattice_xilinx.v(8[17:34])
    OB xilinx_spi_cs_data_pad (.I(xilinx_spi_cs_data_c_c), .O(xilinx_spi_cs_data));   // d:/cubemx/stm32h743_xc7a100_ps_configuration/stm32_lattice_xilinx.v(9[17:35])
    OB xilinx_spi_clk_pad (.I(xilinx_spi_clk_c_c), .O(xilinx_spi_clk));   // d:/cubemx/stm32h743_xc7a100_ps_configuration/stm32_lattice_xilinx.v(10[17:31])
    OB xilinx_spi_mosi_pad (.I(xilinx_spi_mosi_c_c), .O(xilinx_spi_mosi));   // d:/cubemx/stm32h743_xc7a100_ps_configuration/stm32_lattice_xilinx.v(11[17:32])
    IB xilinx_spi_cs_cmd_c_pad (.I(stm32_spi_cs_cmd), .O(xilinx_spi_cs_cmd_c_c));   // d:/cubemx/stm32h743_xc7a100_ps_configuration/stm32_lattice_xilinx.v(2[17:33])
    IB xilinx_spi_cs_data_c_pad (.I(stm32_spi_cs_data), .O(xilinx_spi_cs_data_c_c));   // d:/cubemx/stm32h743_xc7a100_ps_configuration/stm32_lattice_xilinx.v(3[17:34])
    IB xilinx_spi_clk_c_pad (.I(stm32_spi_clk), .O(xilinx_spi_clk_c_c));   // d:/cubemx/stm32h743_xc7a100_ps_configuration/stm32_lattice_xilinx.v(4[17:30])
    IB xilinx_spi_mosi_c_pad (.I(stm32_spi_mosi), .O(xilinx_spi_mosi_c_c));   // d:/cubemx/stm32h743_xc7a100_ps_configuration/stm32_lattice_xilinx.v(5[17:31])
    IB stm32_spi_miso_c_pad (.I(xilinx_spi_miso), .O(stm32_spi_miso_c_c));   // d:/cubemx/stm32h743_xc7a100_ps_configuration/stm32_lattice_xilinx.v(12[17:32])
    GSR GSR_INST (.GSR(VCC_net));
    TSALL TSALL_INST (.TSALL(GND_net));
    PUR PUR_INST (.PUR(VCC_net));
    defparam PUR_INST.RST_PULSE = 1;
    VHI i24 (.Z(VCC_net));
    
endmodule
//
// Verilog Description of module TSALL
// module not written out since it is a black-box. 
//

//
// Verilog Description of module PUR
// module not written out since it is a black-box. 
//

