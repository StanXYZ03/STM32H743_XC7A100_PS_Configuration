# GPIO Probe

This is a minimum Vivado test design used as a Xilinx-to-Lattice loopback probe.

Behavior:
- `probe_led` on `E22` toggles every 500 ms using the board clock on `W19`
- `probe_loopback` on `J16` is only kept as an input so the board-side return line is present

Expected test:
- Xilinx `E22` should toggle every 500 ms
- Lattice should bridge `PB27B -> PB24D`
- If the Xilinx -> Lattice -> Xilinx return path is continuous, the `J16` line / LED should toggle every 500 ms

Build:
```powershell
& 'D:\Vivado\Vivado\2018.3\bin\vivado.bat' -mode batch -source 'D:\CubeMX\STM32H743_XC7A100_PS_Configuration\vivado_gpio_probe\create_project.tcl'
```
