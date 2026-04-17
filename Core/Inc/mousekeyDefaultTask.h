#ifndef __MOUSEKEY_DEFAULT_TASK_H__
#define __MOUSEKEY_DEFAULT_TASK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define MOUSEKEY_USB_PACKET_LEN 14U

typedef struct
{
    uint8_t header;
    uint8_t mouse_ctrl;
    uint8_t mouse_wheel;
    int16_t x_coord;
    int16_t y_coord;
    uint8_t key_mod;
    uint8_t key_code[6];
} MouseKeyPacket_t;

extern MouseKeyPacket_t g_mousekey_packet;
extern volatile int16_t g_mousekey_x;
extern volatile int16_t g_mousekey_y;
extern volatile uint32_t g_mousekey_debug_regs[3];
extern volatile uint32_t g_mousekey_debug_loop_count;
extern volatile uint32_t g_mousekey_debug_ok_count;
extern volatile uint32_t g_mousekey_debug_fail_count;
extern volatile uint8_t g_mousekey_debug_last_status;

void mousekeyDefaultTask(void const * argument);

#ifdef __cplusplus
}
#endif

#endif /* __MOUSEKEY_DEFAULT_TASK_H__ */
