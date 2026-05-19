#include "ETHDefaultTask.h"
#include "Bsp_ETH.h"
#include "cmsis_os.h"
#include "lwip.h"
#include "lwip/inet.h"
#include "lwip/netif.h"
#include "lwip/sockets.h"
#include "lwip/errno.h"
#include <stdio.h>
#include <string.h>

extern struct netif gnetif;

#ifndef ETH_STARTUP_DELAY_MS
#define ETH_STARTUP_DELAY_MS 3000U
#endif

typedef struct
{
    uint32_t magic;
    uint32_t build_marker;
    uint32_t started;
    uint32_t startup_delay_done;
    uint32_t lwip_init_enter;
    uint32_t lwip_init_done;
    uint32_t loop_count;
    uint32_t wait_loop_count;
    uint32_t network_ready;
    uint32_t network_ready_mask;
    uint32_t network_missing_mask;
    uint32_t fill_server_enter;
    uint32_t fill_server_exit;
    uint32_t server_ip_word;
    uint32_t server_port;
    uint32_t socket_create_count;
    uint32_t send_count;
    uint32_t send_error_count;
    int32_t last_socket;
    int32_t last_send;
    int32_t last_errno;
    int32_t socket_errno;
    uint32_t last_payload_len;
    uint32_t netif_up;
    uint32_t netif_link_up;
    uint32_t netif_flags;
    uint32_t ip_addr;
    uint32_t netmask_addr;
    uint32_t gw_addr;
    uint32_t send_netif_up;
    uint32_t send_netif_link_up;
    uint32_t send_netif_flags;
    uint32_t heartbeat;
    uint32_t phase;
    uint32_t current_bit;
    uint32_t bit_change_count;
    uint32_t keepalive_count;
} ETH_TaskDebug_t;

volatile ETH_TaskDebug_t eth_task_dbg = {
    0x45544844U, /* "ETHD" */
    0x20260429U
};

static uint8_t ETH_TaskWaitForNetwork(void)
{
    const uint32_t timeout_ms = 30000U;
    uint32_t waited_ms = 0U;

    while (waited_ms < timeout_ms) {
        eth_task_dbg.wait_loop_count++;
        eth_task_dbg.netif_up = netif_is_up(&gnetif) ? 1U : 0U;
        eth_task_dbg.netif_link_up = netif_is_link_up(&gnetif) ? 1U : 0U;
        eth_task_dbg.netif_flags = gnetif.flags;
        eth_task_dbg.ip_addr = ip4_addr_get_u32(netif_ip4_addr(&gnetif));
        eth_task_dbg.netmask_addr = ip4_addr_get_u32(netif_ip4_netmask(&gnetif));
        eth_task_dbg.gw_addr = ip4_addr_get_u32(netif_ip4_gw(&gnetif));
        eth_task_dbg.network_ready_mask =
            (netif_is_up(&gnetif) ? 0x01U : 0U) |
            (netif_is_link_up(&gnetif) ? 0x02U : 0U) |
            (!ip4_addr_isany_val(*netif_ip4_addr(&gnetif)) ? 0x04U : 0U);
        eth_task_dbg.network_missing_mask = eth_task_dbg.network_ready_mask ^ 0x07U;
        eth_task_dbg.heartbeat++;
        eth_task_dbg.phase = 2U;

#if (BSP_ETH_MINIMAL_TEST_FORCE_SEND != 0U)
        if (netif_is_up(&gnetif) &&
            !ip4_addr_isany_val(*netif_ip4_addr(&gnetif))) {
            eth_task_dbg.network_ready = 1U;
            return 1U;
        }
#else
        if (netif_is_up(&gnetif) &&
            netif_is_link_up(&gnetif) &&
            !ip4_addr_isany_val(*netif_ip4_addr(&gnetif))) {
            eth_task_dbg.network_ready = 1U;
            return 1U;
        }
#endif

        osDelay(100U);
        waited_ms += 100U;
    }

    eth_task_dbg.network_ready = 0U;
    return 0U;
}

static int ETH_TaskCreateUdpSocket(void)
{
    int sock = lwip_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    eth_task_dbg.socket_create_count++;
    eth_task_dbg.last_socket = sock;
    eth_task_dbg.socket_errno = (sock < 0) ? errno : 0;
    if (sock < 0) {
        return -1;
    }

    unsigned long nonblocking = 1UL;
    (void)lwip_ioctl(sock, FIONBIO, &nonblocking);

#if (BSP_ETH_MINIMAL_TEST_BROADCAST != 0U)
    int broadcast_enable = 1;
    (void)lwip_setsockopt(sock,
                          SOL_SOCKET,
                          SO_BROADCAST,
                          &broadcast_enable,
                          sizeof(broadcast_enable));
#endif

    return sock;
}

static void ETH_TaskSnapshotNetifBeforeSend(void)
{
    eth_task_dbg.send_netif_up = netif_is_up(&gnetif) ? 1U : 0U;
    eth_task_dbg.send_netif_link_up = netif_is_link_up(&gnetif) ? 1U : 0U;
    eth_task_dbg.send_netif_flags = gnetif.flags;
}

static void ETH_TaskFillServerAddress(struct sockaddr_in *server_addr)
{
    uint8_t ip[4] = {0};

    eth_task_dbg.phase = 10U;
    eth_task_dbg.fill_server_enter++;
    Bsp_ETH_GetServerIp(ip);
    eth_task_dbg.server_ip_word = ((uint32_t)ip[0] << 24) |
                                  ((uint32_t)ip[1] << 16) |
                                  ((uint32_t)ip[2] << 8)  |
                                  ((uint32_t)ip[3]);
    eth_task_dbg.server_port = Bsp_ETH_GetServerPort();

    memset(server_addr, 0, sizeof(*server_addr));
    server_addr->sin_family = AF_INET;
    server_addr->sin_port = PP_HTONS((uint16_t)eth_task_dbg.server_port);
#if (BSP_ETH_MINIMAL_TEST_BROADCAST != 0U)
    server_addr->sin_addr.s_addr = PP_HTONL(0xFFFFFFFFUL);
#else
    server_addr->sin_addr.s_addr = PP_HTONL(eth_task_dbg.server_ip_word);
#endif
    eth_task_dbg.fill_server_exit++;
    eth_task_dbg.phase = 11U;
}

void ETHDefaultTask(void const *argument)
{
    (void)argument;

    eth_task_dbg.started = 1U;
    eth_task_dbg.phase = 1U;
    eth_task_dbg.build_marker = 0x20260429U;

    Bsp_ETH_Init();
    osDelay(ETH_STARTUP_DELAY_MS);
    eth_task_dbg.startup_delay_done = 1U;

    eth_task_dbg.lwip_init_enter++;
    eth_task_dbg.phase = 12U;
    MX_LWIP_Init();
    eth_task_dbg.lwip_init_done = 1U;
    eth_task_dbg.phase = 13U;
    osDelay(300U);

    int sock = -1;
    struct sockaddr_in server_addr;

#if (BSP_ETH_MINIMAL_TEST_MODE != 0U)
    uint32_t seq = 0U;

    ETH_TaskFillServerAddress(&server_addr);

    for (;;) {
        eth_task_dbg.loop_count++;
        if (!ETH_TaskWaitForNetwork()) {
            eth_task_dbg.phase = 3U;
            osDelay(1000U);
            continue;
        }

        if (sock < 0) {
            eth_task_dbg.phase = 4U;
            sock = ETH_TaskCreateUdpSocket();
            if (sock < 0) {
                eth_task_dbg.phase = 5U;
                osDelay(1000U);
                continue;
            }

        }

        char payload[64];
        int payload_len = snprintf(payload,
                                   sizeof(payload),
                                   "STM32 ETH TEST %lu\r\n",
                                   (unsigned long)seq++);

        ETH_TaskSnapshotNetifBeforeSend();

        int sent = lwip_sendto(sock,
                               payload,
                               (size_t)payload_len,
                               0,
                               (const struct sockaddr *)&server_addr,
                               sizeof(server_addr));

        eth_task_dbg.last_send = sent;
        eth_task_dbg.last_payload_len = (uint32_t)payload_len;
        if (sent < 0) {
            eth_task_dbg.phase = 6U;
            eth_task_dbg.send_error_count++;
            eth_task_dbg.last_errno = errno;
            lwip_close(sock);
            sock = -1;
        } else {
            eth_task_dbg.phase = 7U;
            eth_task_dbg.last_errno = 0;
            eth_task_dbg.send_count++;
        }

        osDelay(BSP_ETH_MINIMAL_TEST_PERIOD_MS);
    }
#else
    uint8_t last_bit = 0xFFU;
    uint32_t keepalive_elapsed = Bsp_ETH_GetKeepalivePeriodMs();

    ETH_TaskFillServerAddress(&server_addr);

    for (;;) {
        eth_task_dbg.loop_count++;
        if (!ETH_TaskWaitForNetwork()) {
            osDelay(1000U);
            continue;
        }

        if (sock < 0) {
            sock = ETH_TaskCreateUdpSocket();
            if (sock < 0) {
                osDelay(1000U);
                continue;
            }
        }

        uint8_t bit = Bsp_ETH_ReadDataBit();
        uint32_t sample_period = Bsp_ETH_GetSamplePeriodMs();
        uint32_t keepalive_period = Bsp_ETH_GetKeepalivePeriodMs();
        eth_task_dbg.current_bit = bit;

        if ((bit != last_bit) || (keepalive_elapsed >= keepalive_period)) {
            char payload[4];
            uint16_t payload_len = Bsp_ETH_FormatBitPayload(bit, payload, sizeof(payload));

            if (payload_len > 0U) {
                ETH_TaskSnapshotNetifBeforeSend();

                int sent = lwip_sendto(sock,
                                       payload,
                                       payload_len,
                                       0,
                                       (const struct sockaddr *)&server_addr,
                                       sizeof(server_addr));
                eth_task_dbg.last_payload_len = (uint32_t)payload_len;
                if (sent < 0) {
                    eth_task_dbg.phase = 6U;
                    eth_task_dbg.send_error_count++;
                    eth_task_dbg.last_send = sent;
                    eth_task_dbg.last_errno = errno;
                    lwip_close(sock);
                    sock = -1;
                } else {
                    eth_task_dbg.phase = 7U;
                    eth_task_dbg.last_send = sent;
                    eth_task_dbg.last_errno = 0;
                    eth_task_dbg.send_count++;
                    if (bit != last_bit) {
                        eth_task_dbg.bit_change_count++;
                    } else {
                        eth_task_dbg.keepalive_count++;
                    }
                    last_bit = bit;
                    keepalive_elapsed = 0U;
                }
            }
        }

        osDelay(sample_period);
        keepalive_elapsed += sample_period;
    }
#endif
}
