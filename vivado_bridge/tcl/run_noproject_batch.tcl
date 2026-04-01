set script_dir [file normalize [file dirname [info script]]]
set repo_root [file normalize [file join $script_dir ".." ".."]]
set output_dir [file join $repo_root "Confg-Flash" "out_noproject"]

file mkdir $output_dir

create_project -in_memory -part xc7a100tfgg484-2

set_property target_language Verilog [current_project]
set_property default_lib xil_defaultlib [current_project]

read_verilog [file join $repo_root "vivado_bridge" "rtl" "jtag_user_bridge.v"]
read_verilog [file join $repo_root "vivado_bridge" "rtl" "spi_flash_bridge_core.v"]
read_verilog [file join $repo_root "vivado_bridge" "rtl" "spi_flash_bridge_top.v"]
read_xdc     [file join $repo_root "vivado_bridge" "xdc" "xc7a100tfgg484_spi_flash_bridge.xdc"]

synth_design -top spi_flash_bridge_top -part xc7a100tfgg484-2
opt_design
place_design
route_design

report_timing_summary -file [file join $output_dir "timing_summary.rpt"]
report_utilization    -file [file join $output_dir "utilization.rpt"]

set_property CFGBVS VCCO [current_design]
set_property CONFIG_VOLTAGE 3.3 [current_design]
set_property BITSTREAM.GENERAL.COMPRESS TRUE [current_design]
set_property BITSTREAM.CONFIG.SPI_BUSWIDTH 1 [current_design]

set bit_prefix [file join $output_dir "spi_flash_bridge"]
write_bitstream -force -bin_file $bit_prefix

puts "Generated outputs:"
puts "  [file normalize ${bit_prefix}.bit]"
puts "  [file normalize ${bit_prefix}.bin]"
puts "  [file normalize [file join $output_dir {timing_summary.rpt}]]"
puts "  [file normalize [file join $output_dir {utilization.rpt}]]"

close_project
exit 0
