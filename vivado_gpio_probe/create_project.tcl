set script_dir [file normalize [file dirname [info script]]]
set proj_dir   [file join $script_dir build gpio_probe_const_prj]
set proj_name  gpio_probe_const
set part_name  xc7a100tfgg484-2

file mkdir $proj_dir

create_project $proj_name $proj_dir -part $part_name -force

set_property target_language Verilog [current_project]
set_property default_lib xil_defaultlib [current_project]

add_files -norecurse [file join $script_dir rtl gpio_probe_top.v]
add_files -fileset constrs_1 -norecurse [file join $script_dir xdc gpio_probe_top.xdc]

set_property top gpio_probe_top [current_fileset]
update_compile_order -fileset sources_1

launch_runs synth_1 -jobs 4
wait_on_run synth_1
launch_runs impl_1 -to_step write_bitstream -jobs 4
wait_on_run impl_1

set bit_file [file join $proj_dir "${proj_name}.runs" impl_1 gpio_probe_top.bit]
puts "BITSTREAM: $bit_file"
