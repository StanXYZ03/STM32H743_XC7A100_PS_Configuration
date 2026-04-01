set script_dir [file normalize [file dirname [info script]]]
set repo_root [file normalize [file join $script_dir ".." ".."]]
set project_path [file join $repo_root "Confg-Flash" "Confg-Flash.xpr"]
set output_dir [file join $repo_root "Confg-Flash" "out"]

file mkdir $output_dir

open_project $project_path

if {[get_runs synth_1] ne ""} {
    reset_run synth_1
}

if {[get_runs impl_1] ne ""} {
    reset_run impl_1
}

launch_runs synth_1 -jobs 4
wait_on_run synth_1

set synth_status [get_property STATUS [get_runs synth_1]]
puts "synth_1 status: $synth_status"
if {![string match "*Complete*" $synth_status]} {
    error "Synthesis did not complete successfully."
}

launch_runs impl_1 -to_step write_bitstream -jobs 4
wait_on_run impl_1

set impl_status [get_property STATUS [get_runs impl_1]]
puts "impl_1 status: $impl_status"
if {![string match "*Complete*" $impl_status]} {
    error "Implementation did not complete successfully."
}

open_run impl_1

set bit_prefix [file join $output_dir "spi_flash_bridge"]
write_bitstream -force -bin_file $bit_prefix

puts "Generated outputs:"
puts "  [file normalize ${bit_prefix}.bit]"
puts "  [file normalize ${bit_prefix}.bin]"

close_project
exit 0
