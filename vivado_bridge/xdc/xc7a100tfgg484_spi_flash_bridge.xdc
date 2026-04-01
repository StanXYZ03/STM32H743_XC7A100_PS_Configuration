set_property PACKAGE_PIN W19 [get_ports clk_100MHz]
set_property IOSTANDARD LVCMOS33 [get_ports clk_100MHz]
create_clock -period 10.000 -name clk_100MHz [get_ports clk_100MHz]

set_property PACKAGE_PIN P22 [get_ports spi_io0_io]
set_property PACKAGE_PIN R22 [get_ports spi_io1_io]
set_property PACKAGE_PIN P21 [get_ports spi_io2_io]
set_property PACKAGE_PIN R21 [get_ports spi_io3_io]
set_property PACKAGE_PIN T19 [get_ports spi_cs_o]
set_property IOSTANDARD LVCMOS33 [get_ports {spi_io0_io spi_io1_io spi_io2_io spi_io3_io spi_cs_o}]

# CCLK is driven through STARTUPE2.USRCCLKO and must not be constrained as a regular top-level port.
# INIT_B / PROGRAM_B / DONE remain dedicated configuration pins and are not part of this bridge top.
