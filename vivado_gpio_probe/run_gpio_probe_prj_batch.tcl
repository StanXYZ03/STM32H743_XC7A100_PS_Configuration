open_project "D:/CubeMX/STM32H743_XC7A100_PS_Configuration/vivado_gpio_probe/build/gpio_probe_prj/gpio_probe.xpr"
reset_run synth_1
reset_run impl_1
launch_runs synth_1 -jobs 4
wait_on_run synth_1
launch_runs impl_1 -to_step write_bitstream -jobs 4
wait_on_run impl_1
close_project
exit
