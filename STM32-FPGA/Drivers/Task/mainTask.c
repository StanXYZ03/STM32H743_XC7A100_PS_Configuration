#include "mainTask.h"

void Start_mainTask(void const * argument)
{
	for(;;)
  {
		//STM32在线指示灯
	HAL_GPIO_TogglePin(GPIOI,GPIO_PIN_3);
	osDelay(500);
	}
}
