set_property CFGBVS VCCO [current_design]
set_property CONFIG_VOLTAGE 3.3 [current_design]

# W19 board clock
set_property PACKAGE_PIN W19 [get_ports clk]
set_property IOSTANDARD LVCMOS33 [get_ports clk]
create_clock -period 10.000 -name clk_100MHz [get_ports clk]

# E22 drives the source signal toward Lattice
set_property PACKAGE_PIN Y11 [get_ports probe_led]
set_property IOSTANDARD LVCMOS33 [get_ports probe_led]
