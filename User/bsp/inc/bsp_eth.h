#ifndef __BSP_ETH_H
#define __BSP_ETH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

/*
 * Minimal remote-lab Ethernet test configuration.
 *
 * Board data path:
 *   FPGA threshold bit -> STM32 GPIO -> UDP -> Node.js server -> browser
 *
 * Update these values to match your PC/server and the actual PCB GPIO.
 */
#ifndef BSP_ETH_MINIMAL_TEST_MODE
#define BSP_ETH_MINIMAL_TEST_MODE          0U
#endif

#ifndef BSP_ETH_MINIMAL_TEST_FORCE_SEND
#define BSP_ETH_MINIMAL_TEST_FORCE_SEND    0U
#endif

#ifndef BSP_ETH_MINIMAL_TEST_BROADCAST
#define BSP_ETH_MINIMAL_TEST_BROADCAST     0U
#endif

#ifndef BSP_ETH_SERVER_IP0
#define BSP_ETH_SERVER_IP0                 192U
#endif
#ifndef BSP_ETH_SERVER_IP1
#define BSP_ETH_SERVER_IP1                 168U
#endif
#ifndef BSP_ETH_SERVER_IP2
#define BSP_ETH_SERVER_IP2                 16U
#endif
#ifndef BSP_ETH_SERVER_IP3
#define BSP_ETH_SERVER_IP3                 210U
#endif

#ifndef BSP_ETH_SERVER_PORT
#define BSP_ETH_SERVER_PORT                5005U
#endif

#ifndef BSP_ETH_MINIMAL_TEST_PERIOD_MS
#define BSP_ETH_MINIMAL_TEST_PERIOD_MS     1000U
#endif

#ifndef BSP_ETH_SAMPLE_PERIOD_MS
#define BSP_ETH_SAMPLE_PERIOD_MS           20U
#endif

#ifndef BSP_ETH_KEEPALIVE_PERIOD_MS
#define BSP_ETH_KEEPALIVE_PERIOD_MS        1000U
#endif

/*
 * Default input pin. Change these two macros to the STM32 pin connected to
 * the FPGA threshold output. PC13 is only a safe CubeMX-configured placeholder.
 */
#ifndef BSP_ETH_DATA_GPIO_PORT
#define BSP_ETH_DATA_GPIO_PORT             GPIOG
#endif

#ifndef BSP_ETH_DATA_GPIO_PIN
#define BSP_ETH_DATA_GPIO_PIN              GPIO_PIN_9
#endif

#ifndef BSP_ETH_DATA_ACTIVE_HIGH
#define BSP_ETH_DATA_ACTIVE_HIGH           1U
#endif

/*
 * When enabled, ETH test mode takes ownership of PG9 as a plain GPIO input.
 * This disables the SPI1-based mouse/key FPGA readback path at runtime so the
 * existing Ethernet PG9 sampling logic can run without pin-function conflicts.
 */
#ifndef BSP_ETH_PG9_GPIO_TEST_MODE
#define BSP_ETH_PG9_GPIO_TEST_MODE         0U
#endif

void Bsp_ETH_Init(void);
uint8_t Bsp_ETH_ReadDataBit(void);
uint16_t Bsp_ETH_GetServerPort(void);
uint32_t Bsp_ETH_GetSamplePeriodMs(void);
uint32_t Bsp_ETH_GetKeepalivePeriodMs(void);
void Bsp_ETH_GetServerIp(uint8_t ip[4]);
uint16_t Bsp_ETH_FormatBitPayload(uint8_t bit, char *buffer, uint16_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_ETH_H */
