#ifndef UARTTASK_H
#define UARTTASK_H

#include "usart.h"
#include "cmsis_os.h"
#include "usart_mouse_key.h"
#include "mouse_mapping.h"
#include "spiTask.h"
#include "key.h"


extern void Start_uartTask(void const * argument);
#endif
