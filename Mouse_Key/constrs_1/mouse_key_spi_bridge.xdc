# Mouse_Key SPI bridge constraints for xc7a100tfgg484-2
# These constraints match the ch9350_spi_top top-level port names.

set_property CFGBVS VCCO [current_design]
set_property CONFIG_VOLTAGE 3.3 [current_design]

set_property PACKAGE_PIN W19 [get_ports clk]
set_property IOSTANDARD LVCMOS33 [get_ports clk]
create_clock -period 10.000 -name clk_100MHz [get_ports clk]

set_property PACKAGE_PIN T6 [get_ports rst_n]
set_property PACKAGE_PIN E21 [get_ports ch9350_txd]

set_property PACKAGE_PIN G17 [get_ports spi_cs_data]
set_property PACKAGE_PIN F20 [get_ports spi_sdo]
set_property PACKAGE_PIN J19 [get_ports spi_cs_cmd]
set_property PACKAGE_PIN J20 [get_ports spi_scl]
set_property PACKAGE_PIN G18 [get_ports spi_sdi]

set_property IOSTANDARD LVCMOS33 [get_ports rst_n]
set_property IOSTANDARD LVCMOS33 [get_ports ch9350_txd]
set_property IOSTANDARD LVCMOS33 [get_ports spi_cs_cmd]
set_property IOSTANDARD LVCMOS33 [get_ports spi_cs_data]
set_property IOSTANDARD LVCMOS33 [get_ports spi_scl]
set_property IOSTANDARD LVCMOS33 [get_ports spi_sdi]
set_property IOSTANDARD LVCMOS33 [get_ports spi_sdo]


create_debug_core u_ila_0 ila
set_property ALL_PROBE_SAME_MU true [get_debug_cores u_ila_0]
set_property ALL_PROBE_SAME_MU_CNT 1 [get_debug_cores u_ila_0]
set_property C_ADV_TRIGGER false [get_debug_cores u_ila_0]
set_property C_DATA_DEPTH 4096 [get_debug_cores u_ila_0]
set_property C_EN_STRG_QUAL false [get_debug_cores u_ila_0]
set_property C_INPUT_PIPE_STAGES 0 [get_debug_cores u_ila_0]
set_property C_TRIGIN_EN false [get_debug_cores u_ila_0]
set_property C_TRIGOUT_EN false [get_debug_cores u_ila_0]
set_property port_width 1 [get_debug_ports u_ila_0/clk]
connect_debug_port u_ila_0/clk [get_nets [list clk_IBUF_BUFG]]
set_property PROBE_TYPE DATA_AND_TRIGGER [get_debug_ports u_ila_0/probe0]
set_property port_width 3 [get_debug_ports u_ila_0/probe0]
connect_debug_port u_ila_0/probe0 [get_nets [list {u_spi_comm/u_spi/u_spi_cmd_data/u_spi_data_channel/spi_cs_reg[0]} {u_spi_comm/u_spi/u_spi_cmd_data/u_spi_data_channel/spi_cs_reg[1]} {u_spi_comm/u_spi/u_spi_cmd_data/u_spi_data_channel/spi_cs_reg[2]}]]
create_debug_port u_ila_0 probe
set_property PROBE_TYPE DATA_AND_TRIGGER [get_debug_ports u_ila_0/probe1]
set_property port_width 3 [get_debug_ports u_ila_0/probe1]
connect_debug_port u_ila_0/probe1 [get_nets [list {u_spi_comm/u_spi/u_spi_cmd_data/cs_cmd_sync[0]} {u_spi_comm/u_spi/u_spi_cmd_data/cs_cmd_sync[1]} {u_spi_comm/u_spi/u_spi_cmd_data/cs_cmd_sync[2]}]]
create_debug_port u_ila_0 probe
set_property PROBE_TYPE DATA_AND_TRIGGER [get_debug_ports u_ila_0/probe2]
set_property port_width 4 [get_debug_ports u_ila_0/probe2]
connect_debug_port u_ila_0/probe2 [get_nets [list {u_spi_comm/u_spi/u_spi_cmd_data/curr_ch_addr[0]} {u_spi_comm/u_spi/u_spi_cmd_data/curr_ch_addr[1]} {u_spi_comm/u_spi/u_spi_cmd_data/curr_ch_addr[2]} {u_spi_comm/u_spi/u_spi_cmd_data/curr_ch_addr[3]}]]
create_debug_port u_ila_0 probe
set_property PROBE_TYPE DATA_AND_TRIGGER [get_debug_ports u_ila_0/probe3]
set_property port_width 8 [get_debug_ports u_ila_0/probe3]
connect_debug_port u_ila_0/probe3 [get_nets [list {u_spi_comm/u_spi/u_spi_cmd_data/cmd_addr[0]} {u_spi_comm/u_spi/u_spi_cmd_data/cmd_addr[1]} {u_spi_comm/u_spi/u_spi_cmd_data/cmd_addr[2]} {u_spi_comm/u_spi/u_spi_cmd_data/cmd_addr[3]} {u_spi_comm/u_spi/u_spi_cmd_data/cmd_addr[4]} {u_spi_comm/u_spi/u_spi_cmd_data/cmd_addr[5]} {u_spi_comm/u_spi/u_spi_cmd_data/cmd_addr[6]} {u_spi_comm/u_spi/u_spi_cmd_data/cmd_addr[7]}]]
create_debug_port u_ila_0 probe
set_property PROBE_TYPE DATA_AND_TRIGGER [get_debug_ports u_ila_0/probe4]
set_property port_width 32 [get_debug_ports u_ila_0/probe4]
connect_debug_port u_ila_0/probe4 [get_nets [list {u_spi_comm/u_spi/u_spi_cmd_data/rx_data[0]} {u_spi_comm/u_spi/u_spi_cmd_data/rx_data[1]} {u_spi_comm/u_spi/u_spi_cmd_data/rx_data[2]} {u_spi_comm/u_spi/u_spi_cmd_data/rx_data[3]} {u_spi_comm/u_spi/u_spi_cmd_data/rx_data[4]} {u_spi_comm/u_spi/u_spi_cmd_data/rx_data[5]} {u_spi_comm/u_spi/u_spi_cmd_data/rx_data[6]} {u_spi_comm/u_spi/u_spi_cmd_data/rx_data[7]} {u_spi_comm/u_spi/u_spi_cmd_data/rx_data[8]} {u_spi_comm/u_spi/u_spi_cmd_data/rx_data[9]} {u_spi_comm/u_spi/u_spi_cmd_data/rx_data[10]} {u_spi_comm/u_spi/u_spi_cmd_data/rx_data[11]} {u_spi_comm/u_spi/u_spi_cmd_data/rx_data[12]} {u_spi_comm/u_spi/u_spi_cmd_data/rx_data[13]} {u_spi_comm/u_spi/u_spi_cmd_data/rx_data[14]} {u_spi_comm/u_spi/u_spi_cmd_data/rx_data[15]} {u_spi_comm/u_spi/u_spi_cmd_data/rx_data[16]} {u_spi_comm/u_spi/u_spi_cmd_data/rx_data[17]} {u_spi_comm/u_spi/u_spi_cmd_data/rx_data[18]} {u_spi_comm/u_spi/u_spi_cmd_data/rx_data[19]} {u_spi_comm/u_spi/u_spi_cmd_data/rx_data[20]} {u_spi_comm/u_spi/u_spi_cmd_data/rx_data[21]} {u_spi_comm/u_spi/u_spi_cmd_data/rx_data[22]} {u_spi_comm/u_spi/u_spi_cmd_data/rx_data[23]} {u_spi_comm/u_spi/u_spi_cmd_data/rx_data[24]} {u_spi_comm/u_spi/u_spi_cmd_data/rx_data[25]} {u_spi_comm/u_spi/u_spi_cmd_data/rx_data[26]} {u_spi_comm/u_spi/u_spi_cmd_data/rx_data[27]} {u_spi_comm/u_spi/u_spi_cmd_data/rx_data[28]} {u_spi_comm/u_spi/u_spi_cmd_data/rx_data[29]} {u_spi_comm/u_spi/u_spi_cmd_data/rx_data[30]} {u_spi_comm/u_spi/u_spi_cmd_data/rx_data[31]}]]
create_debug_port u_ila_0 probe
set_property PROBE_TYPE DATA_AND_TRIGGER [get_debug_ports u_ila_0/probe5]
set_property port_width 32 [get_debug_ports u_ila_0/probe5]
connect_debug_port u_ila_0/probe5 [get_nets [list {u_spi_comm/u_spi/u_spi_cmd_data/tx_data[0]} {u_spi_comm/u_spi/u_spi_cmd_data/tx_data[1]} {u_spi_comm/u_spi/u_spi_cmd_data/tx_data[2]} {u_spi_comm/u_spi/u_spi_cmd_data/tx_data[3]} {u_spi_comm/u_spi/u_spi_cmd_data/tx_data[4]} {u_spi_comm/u_spi/u_spi_cmd_data/tx_data[5]} {u_spi_comm/u_spi/u_spi_cmd_data/tx_data[6]} {u_spi_comm/u_spi/u_spi_cmd_data/tx_data[7]} {u_spi_comm/u_spi/u_spi_cmd_data/tx_data[8]} {u_spi_comm/u_spi/u_spi_cmd_data/tx_data[9]} {u_spi_comm/u_spi/u_spi_cmd_data/tx_data[10]} {u_spi_comm/u_spi/u_spi_cmd_data/tx_data[11]} {u_spi_comm/u_spi/u_spi_cmd_data/tx_data[12]} {u_spi_comm/u_spi/u_spi_cmd_data/tx_data[13]} {u_spi_comm/u_spi/u_spi_cmd_data/tx_data[14]} {u_spi_comm/u_spi/u_spi_cmd_data/tx_data[15]} {u_spi_comm/u_spi/u_spi_cmd_data/tx_data[16]} {u_spi_comm/u_spi/u_spi_cmd_data/tx_data[17]} {u_spi_comm/u_spi/u_spi_cmd_data/tx_data[18]} {u_spi_comm/u_spi/u_spi_cmd_data/tx_data[19]} {u_spi_comm/u_spi/u_spi_cmd_data/tx_data[20]} {u_spi_comm/u_spi/u_spi_cmd_data/tx_data[21]} {u_spi_comm/u_spi/u_spi_cmd_data/tx_data[22]} {u_spi_comm/u_spi/u_spi_cmd_data/tx_data[23]} {u_spi_comm/u_spi/u_spi_cmd_data/tx_data[24]} {u_spi_comm/u_spi/u_spi_cmd_data/tx_data[25]} {u_spi_comm/u_spi/u_spi_cmd_data/tx_data[26]} {u_spi_comm/u_spi/u_spi_cmd_data/tx_data[27]} {u_spi_comm/u_spi/u_spi_cmd_data/tx_data[28]} {u_spi_comm/u_spi/u_spi_cmd_data/tx_data[29]} {u_spi_comm/u_spi/u_spi_cmd_data/tx_data[30]} {u_spi_comm/u_spi/u_spi_cmd_data/tx_data[31]}]]
create_debug_port u_ila_0 probe
set_property PROBE_TYPE DATA_AND_TRIGGER [get_debug_ports u_ila_0/probe6]
set_property port_width 32 [get_debug_ports u_ila_0/probe6]
connect_debug_port u_ila_0/probe6 [get_nets [list {u_spi_comm/write_reg_2[0]} {u_spi_comm/write_reg_2[1]} {u_spi_comm/write_reg_2[2]} {u_spi_comm/write_reg_2[3]} {u_spi_comm/write_reg_2[4]} {u_spi_comm/write_reg_2[5]} {u_spi_comm/write_reg_2[6]} {u_spi_comm/write_reg_2[7]} {u_spi_comm/write_reg_2[8]} {u_spi_comm/write_reg_2[9]} {u_spi_comm/write_reg_2[10]} {u_spi_comm/write_reg_2[11]} {u_spi_comm/write_reg_2[12]} {u_spi_comm/write_reg_2[13]} {u_spi_comm/write_reg_2[14]} {u_spi_comm/write_reg_2[15]} {u_spi_comm/write_reg_2[16]} {u_spi_comm/write_reg_2[17]} {u_spi_comm/write_reg_2[18]} {u_spi_comm/write_reg_2[19]} {u_spi_comm/write_reg_2[20]} {u_spi_comm/write_reg_2[21]} {u_spi_comm/write_reg_2[22]} {u_spi_comm/write_reg_2[23]} {u_spi_comm/write_reg_2[24]} {u_spi_comm/write_reg_2[25]} {u_spi_comm/write_reg_2[26]} {u_spi_comm/write_reg_2[27]} {u_spi_comm/write_reg_2[28]} {u_spi_comm/write_reg_2[29]} {u_spi_comm/write_reg_2[30]} {u_spi_comm/write_reg_2[31]}]]
create_debug_port u_ila_0 probe
set_property PROBE_TYPE DATA_AND_TRIGGER [get_debug_ports u_ila_0/probe7]
set_property port_width 32 [get_debug_ports u_ila_0/probe7]
connect_debug_port u_ila_0/probe7 [get_nets [list {reg_data1[0]} {reg_data1[1]} {reg_data1[2]} {reg_data1[3]} {reg_data1[4]} {reg_data1[5]} {reg_data1[6]} {reg_data1[7]} {reg_data1[8]} {reg_data1[9]} {reg_data1[10]} {reg_data1[11]} {reg_data1[12]} {reg_data1[13]} {reg_data1[14]} {reg_data1[15]} {reg_data1[16]} {reg_data1[17]} {reg_data1[18]} {reg_data1[19]} {reg_data1[20]} {reg_data1[21]} {reg_data1[22]} {reg_data1[23]} {reg_data1[24]} {reg_data1[25]} {reg_data1[26]} {reg_data1[27]} {reg_data1[28]} {reg_data1[29]} {reg_data1[30]} {reg_data1[31]}]]
create_debug_port u_ila_0 probe
set_property PROBE_TYPE DATA_AND_TRIGGER [get_debug_ports u_ila_0/probe8]
set_property port_width 32 [get_debug_ports u_ila_0/probe8]
connect_debug_port u_ila_0/probe8 [get_nets [list {u_spi_comm/write_reg_0[0]} {u_spi_comm/write_reg_0[1]} {u_spi_comm/write_reg_0[2]} {u_spi_comm/write_reg_0[3]} {u_spi_comm/write_reg_0[4]} {u_spi_comm/write_reg_0[5]} {u_spi_comm/write_reg_0[6]} {u_spi_comm/write_reg_0[7]} {u_spi_comm/write_reg_0[8]} {u_spi_comm/write_reg_0[9]} {u_spi_comm/write_reg_0[10]} {u_spi_comm/write_reg_0[11]} {u_spi_comm/write_reg_0[12]} {u_spi_comm/write_reg_0[13]} {u_spi_comm/write_reg_0[14]} {u_spi_comm/write_reg_0[15]} {u_spi_comm/write_reg_0[16]} {u_spi_comm/write_reg_0[17]} {u_spi_comm/write_reg_0[18]} {u_spi_comm/write_reg_0[19]} {u_spi_comm/write_reg_0[20]} {u_spi_comm/write_reg_0[21]} {u_spi_comm/write_reg_0[22]} {u_spi_comm/write_reg_0[23]} {u_spi_comm/write_reg_0[24]} {u_spi_comm/write_reg_0[25]} {u_spi_comm/write_reg_0[26]} {u_spi_comm/write_reg_0[27]} {u_spi_comm/write_reg_0[28]} {u_spi_comm/write_reg_0[29]} {u_spi_comm/write_reg_0[30]} {u_spi_comm/write_reg_0[31]}]]
create_debug_port u_ila_0 probe
set_property PROBE_TYPE DATA_AND_TRIGGER [get_debug_ports u_ila_0/probe9]
set_property port_width 32 [get_debug_ports u_ila_0/probe9]
connect_debug_port u_ila_0/probe9 [get_nets [list {reg_data2[0]} {reg_data2[1]} {reg_data2[2]} {reg_data2[3]} {reg_data2[4]} {reg_data2[5]} {reg_data2[6]} {reg_data2[7]} {reg_data2[8]} {reg_data2[9]} {reg_data2[10]} {reg_data2[11]} {reg_data2[12]} {reg_data2[13]} {reg_data2[14]} {reg_data2[15]} {reg_data2[16]} {reg_data2[17]} {reg_data2[18]} {reg_data2[19]} {reg_data2[20]} {reg_data2[21]} {reg_data2[22]} {reg_data2[23]} {reg_data2[24]} {reg_data2[25]} {reg_data2[26]} {reg_data2[27]} {reg_data2[28]} {reg_data2[29]} {reg_data2[30]} {reg_data2[31]}]]
create_debug_port u_ila_0 probe
set_property PROBE_TYPE DATA_AND_TRIGGER [get_debug_ports u_ila_0/probe10]
set_property port_width 8 [get_debug_ports u_ila_0/probe10]
connect_debug_port u_ila_0/probe10 [get_nets [list {uart_byte[0]} {uart_byte[1]} {uart_byte[2]} {uart_byte[3]} {uart_byte[4]} {uart_byte[5]} {uart_byte[6]} {uart_byte[7]}]]
create_debug_port u_ila_0 probe
set_property PROBE_TYPE DATA_AND_TRIGGER [get_debug_ports u_ila_0/probe11]
set_property port_width 32 [get_debug_ports u_ila_0/probe11]
connect_debug_port u_ila_0/probe11 [get_nets [list {reg_data0[0]} {reg_data0[1]} {reg_data0[2]} {reg_data0[3]} {reg_data0[4]} {reg_data0[5]} {reg_data0[6]} {reg_data0[7]} {reg_data0[8]} {reg_data0[9]} {reg_data0[10]} {reg_data0[11]} {reg_data0[12]} {reg_data0[13]} {reg_data0[14]} {reg_data0[15]} {reg_data0[16]} {reg_data0[17]} {reg_data0[18]} {reg_data0[19]} {reg_data0[20]} {reg_data0[21]} {reg_data0[22]} {reg_data0[23]} {reg_data0[24]} {reg_data0[25]} {reg_data0[26]} {reg_data0[27]} {reg_data0[28]} {reg_data0[29]} {reg_data0[30]} {reg_data0[31]}]]
create_debug_port u_ila_0 probe
set_property PROBE_TYPE DATA_AND_TRIGGER [get_debug_ports u_ila_0/probe12]
set_property port_width 32 [get_debug_ports u_ila_0/probe12]
connect_debug_port u_ila_0/probe12 [get_nets [list {u_spi_comm/write_reg_1[0]} {u_spi_comm/write_reg_1[1]} {u_spi_comm/write_reg_1[2]} {u_spi_comm/write_reg_1[3]} {u_spi_comm/write_reg_1[4]} {u_spi_comm/write_reg_1[5]} {u_spi_comm/write_reg_1[6]} {u_spi_comm/write_reg_1[7]} {u_spi_comm/write_reg_1[8]} {u_spi_comm/write_reg_1[9]} {u_spi_comm/write_reg_1[10]} {u_spi_comm/write_reg_1[11]} {u_spi_comm/write_reg_1[12]} {u_spi_comm/write_reg_1[13]} {u_spi_comm/write_reg_1[14]} {u_spi_comm/write_reg_1[15]} {u_spi_comm/write_reg_1[16]} {u_spi_comm/write_reg_1[17]} {u_spi_comm/write_reg_1[18]} {u_spi_comm/write_reg_1[19]} {u_spi_comm/write_reg_1[20]} {u_spi_comm/write_reg_1[21]} {u_spi_comm/write_reg_1[22]} {u_spi_comm/write_reg_1[23]} {u_spi_comm/write_reg_1[24]} {u_spi_comm/write_reg_1[25]} {u_spi_comm/write_reg_1[26]} {u_spi_comm/write_reg_1[27]} {u_spi_comm/write_reg_1[28]} {u_spi_comm/write_reg_1[29]} {u_spi_comm/write_reg_1[30]} {u_spi_comm/write_reg_1[31]}]]
create_debug_port u_ila_0 probe
set_property PROBE_TYPE DATA_AND_TRIGGER [get_debug_ports u_ila_0/probe13]
set_property port_width 1 [get_debug_ports u_ila_0/probe13]
connect_debug_port u_ila_0/probe13 [get_nets [list u_spi_comm/ch9350_frame_valid_d1]]
create_debug_port u_ila_0 probe
set_property PROBE_TYPE DATA_AND_TRIGGER [get_debug_ports u_ila_0/probe14]
set_property port_width 1 [get_debug_ports u_ila_0/probe14]
connect_debug_port u_ila_0/probe14 [get_nets [list kb_mouse_valid]]
create_debug_port u_ila_0 probe
set_property PROBE_TYPE DATA_AND_TRIGGER [get_debug_ports u_ila_0/probe15]
set_property port_width 1 [get_debug_ports u_ila_0/probe15]
connect_debug_port u_ila_0/probe15 [get_nets [list u_spi_comm/reg1_sent]]
create_debug_port u_ila_0 probe
set_property PROBE_TYPE DATA_AND_TRIGGER [get_debug_ports u_ila_0/probe16]
set_property port_width 1 [get_debug_ports u_ila_0/probe16]
connect_debug_port u_ila_0/probe16 [get_nets [list {u_spi_comm/u_spi/u_spi_cmd_data/u_spi_data_channel/Shift_Reg_Din[31]_i_1_n_0}]]
create_debug_port u_ila_0 probe
set_property PROBE_TYPE DATA_AND_TRIGGER [get_debug_ports u_ila_0/probe17]
set_property port_width 1 [get_debug_ports u_ila_0/probe17]
connect_debug_port u_ila_0/probe17 [get_nets [list {u_spi_comm/u_spi/u_spi_cmd_data/u_spi_data_channel/Shift_Reg_DOUT[31]_i_1_n_0}]]
create_debug_port u_ila_0 probe
set_property PROBE_TYPE DATA_AND_TRIGGER [get_debug_ports u_ila_0/probe18]
set_property port_width 1 [get_debug_ports u_ila_0/probe18]
connect_debug_port u_ila_0/probe18 [get_nets [list u_spi_comm/u_spi/u_spi_cmd_data/u_spi_data_channel/spi_cs_neg_flag]]
create_debug_port u_ila_0 probe
set_property PROBE_TYPE DATA_AND_TRIGGER [get_debug_ports u_ila_0/probe19]
set_property port_width 1 [get_debug_ports u_ila_0/probe19]
connect_debug_port u_ila_0/probe19 [get_nets [list u_spi_comm/u_spi/u_spi_cmd_data/u_spi_data_channel/spi_cs_pos_flag]]
create_debug_port u_ila_0 probe
set_property PROBE_TYPE DATA_AND_TRIGGER [get_debug_ports u_ila_0/probe20]
set_property port_width 1 [get_debug_ports u_ila_0/probe20]
connect_debug_port u_ila_0/probe20 [get_nets [list u_spi_comm/u_spi/u_spi_cmd_data/u_spi_data_channel/spi_scl_neg_flag]]
create_debug_port u_ila_0 probe
set_property PROBE_TYPE DATA_AND_TRIGGER [get_debug_ports u_ila_0/probe21]
set_property port_width 1 [get_debug_ports u_ila_0/probe21]
connect_debug_port u_ila_0/probe21 [get_nets [list u_spi_comm/u_spi/u_spi_cmd_data/u_spi_data_channel/spi_scl_pos_flag]]
create_debug_port u_ila_0 probe
set_property PROBE_TYPE DATA_AND_TRIGGER [get_debug_ports u_ila_0/probe22]
set_property port_width 1 [get_debug_ports u_ila_0/probe22]
connect_debug_port u_ila_0/probe22 [get_nets [list u_spi_comm/wheel_cleared]]
set_property C_CLK_INPUT_FREQ_HZ 300000000 [get_debug_cores dbg_hub]
set_property C_ENABLE_CLK_DIVIDER false [get_debug_cores dbg_hub]
set_property C_USER_SCAN_CHAIN 1 [get_debug_cores dbg_hub]
connect_debug_port dbg_hub/clk [get_nets clk_IBUF_BUFG]
