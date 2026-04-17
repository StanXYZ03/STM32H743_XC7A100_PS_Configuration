#include "spiTask.h"

uint32_t rece_date[3] = {0}; 


void Start_spiTask(void const * argument)
{
  /* USER CODE BEGIN Start_spiTask */
  /* Infinite loop */
  for(;;)
  {
		uint8_t j=0;
		// 读取3个通道SPI数据
		for(j = 0; j < 3; j++)
		{
				rece_date[j] = SPI_Communication_Rece_Cmd_Data(j);  // 读取数据
			
			// 仅CH1检测鼠标数据有效性
			if(j == 1)
			{
					Set_Mouse_Data_Valid_Check(rece_date[j]);
			}
		}
		MouseKeyPacket.header = 0x55;
		// 处理有效鼠标数据
		if(mouse_data_valid == 1 && (rece_date[0] == 0x00000101))
		{
			Mouse_Data_Convert_To_Screen(rece_date[1]);  // 计算坐标
			MouseKeyPacket.x_coord = curr_mouse_x;
			MouseKeyPacket.y_coord = curr_mouse_y;
		}
		else if(rece_date[0] == 0x00000101)
		{
			MouseKeyPacket.mouse_ctrl = rece_date[1] >> 24;
		}
			MouseKeyPacket.mouse_wheel = wheel_convert(&rece_date[1]);
		//处理键盘数据
		if (key_mapping(rece_date[0], rece_date[1], rece_date[2], &key_state) == 0) 
		{
			MouseKeyPacket.key_mod = key_state.modifier;
			 for(j = 0; j < 6; j++)
			{	
				MouseKeyPacket.key_code[j] = key_state.key_codes[j];
			}
		}
    osDelay(1);
  }
  /* USER CODE END Start_spiTask */
}
