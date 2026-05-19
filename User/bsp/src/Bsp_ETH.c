#include "Bsp_ETH.h"
#include "spi.h"

void Bsp_ETH_Init(void)
{
#if defined(GPIOA)
    __HAL_RCC_GPIOA_CLK_ENABLE();
#endif
#if defined(GPIOB)
    __HAL_RCC_GPIOB_CLK_ENABLE();
#endif
#if defined(GPIOC)
    __HAL_RCC_GPIOC_CLK_ENABLE();
#endif
#if defined(GPIOD)
    __HAL_RCC_GPIOD_CLK_ENABLE();
#endif
#if defined(GPIOE)
    __HAL_RCC_GPIOE_CLK_ENABLE();
#endif
#if defined(GPIOF)
    __HAL_RCC_GPIOF_CLK_ENABLE();
#endif
#if defined(GPIOG)
    __HAL_RCC_GPIOG_CLK_ENABLE();
#endif
#if defined(GPIOH)
    __HAL_RCC_GPIOH_CLK_ENABLE();
#endif
#if defined(GPIOI)
    __HAL_RCC_GPIOI_CLK_ENABLE();
#endif

#if (BSP_ETH_PG9_GPIO_TEST_MODE != 0U)
    (void)HAL_SPI_DeInit(&hspi1);
#endif

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = BSP_ETH_DATA_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(BSP_ETH_DATA_GPIO_PORT, &GPIO_InitStruct);
}

uint8_t Bsp_ETH_ReadDataBit(void)
{
    GPIO_PinState pin_state = HAL_GPIO_ReadPin(BSP_ETH_DATA_GPIO_PORT,
                                               BSP_ETH_DATA_GPIO_PIN);
    uint8_t bit = (pin_state == GPIO_PIN_SET) ? 1U : 0U;

#if (BSP_ETH_DATA_ACTIVE_HIGH == 0U)
    bit ^= 1U;
#endif

    return bit;
}

uint16_t Bsp_ETH_GetServerPort(void)
{
    return (uint16_t)BSP_ETH_SERVER_PORT;
}

uint32_t Bsp_ETH_GetSamplePeriodMs(void)
{
    return (uint32_t)BSP_ETH_SAMPLE_PERIOD_MS;
}

uint32_t Bsp_ETH_GetKeepalivePeriodMs(void)
{
    return (uint32_t)BSP_ETH_KEEPALIVE_PERIOD_MS;
}

void Bsp_ETH_GetServerIp(uint8_t ip[4])
{
    if (ip == 0) {
        return;
    }

    ip[0] = (uint8_t)BSP_ETH_SERVER_IP0;
    ip[1] = (uint8_t)BSP_ETH_SERVER_IP1;
    ip[2] = (uint8_t)BSP_ETH_SERVER_IP2;
    ip[3] = (uint8_t)BSP_ETH_SERVER_IP3;
}

uint16_t Bsp_ETH_FormatBitPayload(uint8_t bit, char *buffer, uint16_t buffer_size)
{
    if ((buffer == 0) || (buffer_size < 2U)) {
        return 0U;
    }

    buffer[0] = bit ? '1' : '0';
    buffer[1] = '\n';
    return 2U;
}
