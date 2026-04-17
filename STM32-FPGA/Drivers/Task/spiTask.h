#ifndef SPITASK_H
#define SPITASK_H

#include "usart.h"
#include "cmsis_os.h"
#include "spi_communication.h"
#include "delay.h"
#include "mouse_mapping.h"
#include "usart_mouse_key.h"
#include "key.h"


extern uint32_t rece_date[3];    // SPI接收数据

extern void Start_spiTask(void const * argument);

#endif
