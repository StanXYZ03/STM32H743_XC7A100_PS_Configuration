#include "uartTask.h"

void Start_uartTask(void const * argument)
{
  /* USER CODE BEGIN Start_uartTask */
	uint8_t data[] = "Hello, UART!\r\n";
  /* Infinite loop */
  for(;;)
  {
		
		
		USART_Send_MouseKey_Packet(&MouseKeyPacket);

		//HAL_UART_Transmit_DMA(&huart7, data, sizeof(data));
    osDelay(1);
  }
  /* USER CODE END Start_uartTask */
}
