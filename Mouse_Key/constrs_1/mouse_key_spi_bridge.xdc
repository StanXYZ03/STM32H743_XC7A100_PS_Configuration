# Mouse_Key SPI bridge constraints for xc7a100tfgg484-2
# These constraints match the ch9350_spi_top top-level port names.

set_property CFGBVS VCCO [current_design]
set_property CONFIG_VOLTAGE 3.3 [current_design]

set_property PACKAGE_PIN W19 [get_ports clk]
set_property IOSTANDARD LVCMOS33 [get_ports clk]
create_clock -period 10.000 -name clk_100MHz [get_ports clk]

set_property PACKAGE_PIN T6 [get_ports rst_n]
set_property PACKAGE_PIN E21 [get_ports ch9350_txd]

set_property PACKAGE_PIN J16 [get_ports spi_cs_cmd]
set_property PACKAGE_PIN E22 [get_ports spi_cs_data]
set_property PACKAGE_PIN D21 [get_ports spi_scl]
set_property PACKAGE_PIN E19 [get_ports spi_sdi]
set_property PACKAGE_PIN F18 [get_ports spi_sdo]

set_property IOSTANDARD LVCMOS33 [get_ports {rst_n ch9350_txd spi_cs_cmd spi_cs_data spi_scl spi_sdi spi_sdo}]
