set script_dir [file normalize [file dirname [info script]]]
set repo_root [file normalize [file join $script_dir ".." ".."]]
set sim_dir [file join $repo_root "vivado_bridge" "sim_out" "tb_spi_flash_bridge_core"]

file mkdir $sim_dir
cd $sim_dir

set rtl_core  [file join $repo_root "vivado_bridge" "rtl" "spi_flash_bridge_core.v"]
set tb_flash  [file join $repo_root "vivado_bridge" "tb"  "spi_flash_jedec_model.v"]
set tb_top    [file join $repo_root "vivado_bridge" "tb"  "tb_spi_flash_bridge_core.v"]

exec xvlog -sv $rtl_core $tb_flash $tb_top
exec xelab tb_spi_flash_bridge_core -debug typical -s tb_spi_flash_bridge_core
exec xsim tb_spi_flash_bridge_core -runall

puts "Simulation outputs:"
puts "  [file normalize [file join $sim_dir {xvlog.log}]]"
puts "  [file normalize [file join $sim_dir {elaborate.log}]]"
puts "  [file normalize [file join $sim_dir {simulate.log}]]"
puts "  [file normalize [file join $sim_dir {tb_spi_flash_bridge_core.vcd}]]"

exit 0
