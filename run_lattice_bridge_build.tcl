prj_project open "D:/CubeMX/STM32H743_XC7A100_PS_Configuration/STM32_Lattice_Xilinx.ldf"
prj_impl active "STM32_impl"

foreach step {"Synthesis" "Map" "PAR"} {
    puts "Running $step ..."
    prj_run $step -impl "STM32_impl" -forceAll
}

prj_project close
exit
