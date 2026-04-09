#include "bsp.h"
#include "bsp_ui_io_expander.h"
#include "ui_app_config.h"
#include "ui_nav.h"
#include "UI_WaveformCtrl.h"
#include "FPGAConfigDefaultTask.h"

static UI_ScreenId_t    s_screen = UI_SCR_SCOPE;
static volatile uint8_t s_redraw_req;
static int              s_menu_focus;
static int              s_fpga_mode_focus;

/* EC1: GPB2=A GPB1=B GPB0=R；EC2: GPB5=A GPB4=B GPB3=R（跳线切到 EC2 后有效） */
#define EC1_AB_SHIFT     1
#define EC2_AB_SHIFT     4

/* 按键连发间隔（ms），兼作去抖 */
#define BTN_COOLDOWN_MS  180u
/* EC11 按键：连续 N 次采样一致才翻转滤波状态，避免按住时毛刺被当成「松手」导致长按在按住阶段误触发 */
#define EC_BTN_DEBOUNCE_MATCHES  2u

/* 波形页：长按阈值（V/div、ms/div） */
#define EC_SCOPE_LONG_MS 550u

/* 正交状态转移 → -1 / 0 / +1（与常见 Gray 码表一致） */
static const int8_t s_quad_tbl[16] = {
    0, -1, 1, 0,
    1, 0, 0, -1,
    -1, 0, 0, 1,
    0, 1, -1, 0
};

static uint8_t s_ec1_prev_ab;
static uint8_t s_ec2_prev_ab;
static uint8_t s_ec1_btn_prev;
static uint8_t s_ec2_btn_prev;
static uint8_t s_ec1_btn_filt;
static uint8_t s_ec2_btn_filt;
static uint8_t s_ec1_btn_debounce;
static uint8_t s_ec2_btn_debounce;
static uint8_t s_ec_btn_db_arm;
static TickType_t s_ec1_last_ms;
static TickType_t s_ec2_last_ms;
static uint8_t s_io_inited;
static uint8_t s_enc_arm;
static int16_t s_ec1_pulse_accum;
static int16_t s_ec2_pulse_accum;
/* 上次有效单步方向 ±1，用于快转时 A/B 同时跳变（对角）时补 ±2 步 */
static int8_t  s_ec1_last_step;
static int8_t  s_ec2_last_step;

/* 波形页 EC1/EC2：按下时刻与组合判定（松手时再决定是否执行原长按） */
static TickType_t s_ec1_scope_press_tick;
static TickType_t s_ec2_scope_press_tick;
/* 本段按下内若发生过对侧旋钮旋转 → 组合模式，松手时不执行本键原长按 */
static uint8_t    s_ec1_scope_combo_lock;
static uint8_t    s_ec2_scope_combo_lock;

static int nav_is_fpga_mode_page(UI_ScreenId_t screen)
{
    return (screen == UI_SCR_FPGA_SLAVE_SERIAL) ||
           (screen == UI_SCR_FPGA_JTAG_SRAM) ||
           (screen == UI_SCR_FPGA_JTAG_FLASH);
}

static void nav_menu_enter_from_focus(void)
{
    switch (s_menu_focus) {
    case 0:
        s_screen = UI_SCR_SCOPE;
        break;
    case 1:
        s_screen = UI_SCR_CHANNEL;
        break;
    case 2:
        s_screen = UI_SCR_TRIGGER;
        break;
    case 3:
        s_screen = UI_SCR_MEASURE;
        break;
    case 4:
        s_screen = UI_SCR_SYSTEM;
        break;
    case 5:
        s_screen = UI_SCR_FPGA_CONFIG;
        break;
    case 6:
        s_screen = UI_SCR_REMOTE_CONTROL;
        break;
    default:
        break;
    }
}

static void nav_enter_fpga_mode_from_focus(void)
{
    switch (s_fpga_mode_focus) {
    case 0:
        s_screen = UI_SCR_FPGA_SLAVE_SERIAL;
        FPGA_UI_SelectMode(FPGA_UI_MODE_SLAVE_SERIAL);
        break;
    case 1:
        s_screen = UI_SCR_FPGA_JTAG_SRAM;
        FPGA_UI_SelectMode(FPGA_UI_MODE_JTAG_SRAM);
        break;
    case 2:
        s_screen = UI_SCR_FPGA_JTAG_FLASH;
        FPGA_UI_SelectMode(FPGA_UI_MODE_JTAG_FLASH);
        break;
    default:
        break;
    }
}

static void nav_apply_ok(void)
{
    if (s_screen == UI_SCR_WELCOME) {
        s_screen = UI_SCR_MAIN_MENU;
        return;
    }
    if (s_screen == UI_SCR_MAIN_MENU) {
        nav_menu_enter_from_focus();
        return;
    }
    if (s_screen == UI_SCR_FPGA_CONFIG) {
        nav_enter_fpga_mode_from_focus();
        return;
    }
    if (nav_is_fpga_mode_page(s_screen)) {
        if (FPGA_UI_GetFlowState() == FPGA_UI_FLOW_BIN_DONE_WAIT_START) {
            FPGA_UI_RequestStart();
        }
        return;
    }
}

static void nav_apply_back(void)
{
    if (s_screen == UI_SCR_WELCOME) {
        return;
    }
    if (s_screen == UI_SCR_MAIN_MENU) {
        s_screen = UI_SCR_WELCOME;
        return;
    }
    if (s_screen == UI_SCR_FPGA_CONFIG) {
        FPGA_UI_ResetSession();
        s_screen = UI_SCR_MAIN_MENU;
        return;
    }
    if (nav_is_fpga_mode_page(s_screen)) {
        FPGA_UI_ResetSession();
        s_screen = UI_SCR_FPGA_CONFIG;
        return;
    }
    s_screen = UI_SCR_MAIN_MENU;
}

void UI_Nav_SetScreen(UI_ScreenId_t id)
{
    if (id < UI_SCR_COUNT) {
        s_screen = id;
    }
}

UI_ScreenId_t UI_Nav_GetScreen(void)
{
    return s_screen;
}

int UI_Nav_GetMenuFocus(void)
{
    if (s_menu_focus < 0) {
        s_menu_focus = 0;
    }
    if (s_menu_focus > 6) {
        s_menu_focus = 6;
    }
    return s_menu_focus;
}

int UI_Nav_GetFpgaModeFocus(void)
{
    if (s_fpga_mode_focus < 0) {
        s_fpga_mode_focus = 0;
    }
    if (s_fpga_mode_focus > 2) {
        s_fpga_mode_focus = 2;
    }
    return s_fpga_mode_focus;
}

void UI_Nav_MarkDirty(void)
{
    s_redraw_req = 1u;
}

int UI_Nav_ConsumeRedraw(void)
{
    if (s_redraw_req != 0u) {
        s_redraw_req = 0u;
        return 1;
    }
    return 0;
}

void UI_Nav_Poll(void)
{
    uint8_t gpb;
    uint8_t ab1, ab2;
    uint8_t ec1_down;
    uint8_t ec2_down;
    int8_t  step1 = 0;
    int8_t  step2 = 0;
    TickType_t now;

    if (s_io_inited == 0u) {
        BSP_UI_IO_Init();
        s_io_inited = 1u;
    }

    if (BSP_UI_IO_ReadPortB(&gpb) != 0) {
        return;
    }

    {
        uint8_t raw_ec1 = (uint8_t)(((gpb & (1u << 0)) == 0u) ? 1u : 0u);
        uint8_t raw_ec2 = (uint8_t)(((gpb & (1u << 3)) == 0u) ? 1u : 0u);

        if (s_ec_btn_db_arm == 0u) {
            s_ec1_btn_filt     = raw_ec1;
            s_ec2_btn_filt     = raw_ec2;
            s_ec1_btn_debounce = 0u;
            s_ec2_btn_debounce = 0u;
            s_ec_btn_db_arm    = 1u;
        } else {
            if (raw_ec1 == s_ec1_btn_filt) {
                s_ec1_btn_debounce = 0u;
            } else if (++s_ec1_btn_debounce >= EC_BTN_DEBOUNCE_MATCHES) {
                s_ec1_btn_filt     = raw_ec1;
                s_ec1_btn_debounce = 0u;
            }
            if (raw_ec2 == s_ec2_btn_filt) {
                s_ec2_btn_debounce = 0u;
            } else if (++s_ec2_btn_debounce >= EC_BTN_DEBOUNCE_MATCHES) {
                s_ec2_btn_filt     = raw_ec2;
                s_ec2_btn_debounce = 0u;
            }
        }
        ec1_down = s_ec1_btn_filt;
        ec2_down = s_ec2_btn_filt;
    }

    /* 编码器：取 A、B 两相 2bit Gray，边沿判向 */
    ab1 = (uint8_t)(((gpb >> (EC1_AB_SHIFT + 1)) & 1u) << 1) | (uint8_t)(((gpb >> EC1_AB_SHIFT) & 1u));
    ab2 = (uint8_t)(((gpb >> (EC2_AB_SHIFT + 1)) & 1u) << 1) | (uint8_t)(((gpb >> EC2_AB_SHIFT) & 1u));

    now = xTaskGetTickCount();

    if (s_enc_arm == 0u) {
        s_ec1_prev_ab = ab1;
        s_ec2_prev_ab = ab2;
        s_enc_arm       = 1u;
    } else {
        uint8_t old1 = (uint8_t)(s_ec1_prev_ab & 3u);
        uint8_t old2 = (uint8_t)(s_ec2_prev_ab & 3u);

        step1 = s_quad_tbl[(uint8_t)((old1 << 2) | (ab1 & 3u))];
        step2 = s_quad_tbl[(uint8_t)((old2 << 2) | (ab2 & 3u))];
        /* 采样偏慢时易见 00↔11 / 01↔10 对角跳变，表值为 0；按上一有效方向补双倍步进 */
        if (step1 == 0 && ((old1 ^ ab1) & 3u) == 3u && s_ec1_last_step != 0) {
            step1 = (int8_t)(2 * s_ec1_last_step);
        }
        if (step2 == 0 && ((old2 ^ ab2) & 3u) == 3u && s_ec2_last_step != 0) {
            step2 = (int8_t)(2 * s_ec2_last_step);
        }

        s_ec1_prev_ab = ab1;
        s_ec2_prev_ab = ab2;

        if (step1 != 0) {
            s_ec1_last_step = (step1 > 0) ? (int8_t)1 : (int8_t)-1;
        }
        if (step2 != 0) {
            s_ec2_last_step = (step2 > 0) ? (int8_t)1 : (int8_t)-1;
        }
    }

    /*
     * 波形页组合键：只按住 EC2 时转 EC1→solo，应忽略 EC2 轴误码；只按住 EC1 时转 EC2→模式，
     * 应忽略 EC1 轴误码。两键同时按住时不屏蔽，以便两轴仍可分工。
     */
    {
        int8_t s1 = step1;
        int8_t s2 = step2;

        if (s_screen == UI_SCR_SCOPE) {
            if (ec2_down != 0u && ec1_down == 0u) {
                s2 = 0;
            }
            if (ec1_down != 0u && ec2_down == 0u) {
                s1 = 0;
            }
        }

    /* EC1 旋转：主菜单=焦点；波形页= CH1 上下 / 按住 EC2 时循环 DUAL-CH1-CH2 */
    if (s_screen == UI_SCR_MAIN_MENU) {
        if (s_enc_arm != 0u && step1 != 0) {
            s_ec1_pulse_accum += (int16_t)step1;
            while (s_ec1_pulse_accum >= (int16_t)UI_ENC_PULSES_PER_DETENT) {
                s_ec1_pulse_accum -= (int16_t)UI_ENC_PULSES_PER_DETENT;
                if (s_menu_focus < 6) {
                    s_menu_focus++;
                    s_redraw_req = 1u;
                }
            }
            while (s_ec1_pulse_accum <= -(int16_t)UI_ENC_PULSES_PER_DETENT) {
                s_ec1_pulse_accum += (int16_t)UI_ENC_PULSES_PER_DETENT;
                if (s_menu_focus > 0) {
                    s_menu_focus--;
                    s_redraw_req = 1u;
                }
            }
        }
    } else if (s_screen == UI_SCR_FPGA_CONFIG) {
        if (s_enc_arm != 0u && step1 != 0) {
            s_ec1_pulse_accum += (int16_t)step1;
            while (s_ec1_pulse_accum >= (int16_t)UI_ENC_PULSES_PER_DETENT) {
                s_ec1_pulse_accum -= (int16_t)UI_ENC_PULSES_PER_DETENT;
                if (s_fpga_mode_focus < 2) {
                    s_fpga_mode_focus++;
                    s_redraw_req = 1u;
                }
            }
            while (s_ec1_pulse_accum <= -(int16_t)UI_ENC_PULSES_PER_DETENT) {
                s_ec1_pulse_accum += (int16_t)UI_ENC_PULSES_PER_DETENT;
                if (s_fpga_mode_focus > 0) {
                    s_fpga_mode_focus--;
                    s_redraw_req = 1u;
                }
            }
        }
    } else if (s_screen == UI_SCR_SCOPE) {
        if (s_enc_arm != 0u && s1 != 0) {
            s_ec1_pulse_accum += (int16_t)s1;
            while (s_ec1_pulse_accum >= (int16_t)UI_ENC_PULSES_PER_DETENT) {
                s_ec1_pulse_accum -= (int16_t)UI_ENC_PULSES_PER_DETENT;
                if (ec2_down != 0u) {
                    UI_Waveform_CycleSolo();
                } else {
                    UI_Waveform_CH1_MoveDown();
                }
                s_redraw_req = 1u;
            }
            while (s_ec1_pulse_accum <= -(int16_t)UI_ENC_PULSES_PER_DETENT) {
                s_ec1_pulse_accum += (int16_t)UI_ENC_PULSES_PER_DETENT;
                if (ec2_down != 0u) {
                    UI_Waveform_CycleSolo();
                } else {
                    UI_Waveform_CH1_MoveUp();
                }
                s_redraw_req = 1u;
            }
        }
    } else {
        s_ec1_pulse_accum = 0;
        s_ec1_last_step   = 0;
    }

    /* EC2 旋转：波形页= CH2 上下 / 按住 EC1 时 Y-T↔Roll；其它页忽略 */
    if (s_screen == UI_SCR_SCOPE) {
        if (s_enc_arm != 0u && s2 != 0) {
            s_ec2_pulse_accum += (int16_t)s2;
            while (s_ec2_pulse_accum >= (int16_t)UI_ENC_PULSES_PER_DETENT) {
                s_ec2_pulse_accum -= (int16_t)UI_ENC_PULSES_PER_DETENT;
                if (ec1_down != 0u) {
                    UI_Waveform_ToggleDisplayMode();
                } else {
                    UI_Waveform_CH2_MoveDown();
                }
                s_redraw_req = 1u;
            }
            while (s_ec2_pulse_accum <= -(int16_t)UI_ENC_PULSES_PER_DETENT) {
                s_ec2_pulse_accum += (int16_t)UI_ENC_PULSES_PER_DETENT;
                if (ec1_down != 0u) {
                    UI_Waveform_ToggleDisplayMode();
                } else {
                    UI_Waveform_CH2_MoveUp();
                }
                s_redraw_req = 1u;
            }
        }
    } else {
        s_ec2_pulse_accum = 0;
        s_ec2_last_step   = 0;
    }

    } /* scope step mask */

    /*
     * 波形页长按与组合解耦：必须先处理按下沿再置 combo_lock，否则同一周期内
     * 「对侧 step + 本键按下沿」会被按下沿误清。
     * 松手时：combo_lock 则绝不执行原长按；否则按住≥阈值才在松手执行 V/div、ms/div。
     */
    if (s_screen == UI_SCR_SCOPE) {
        if (ec1_down != 0u && s_ec1_btn_prev == 0u) {
            s_ec1_scope_press_tick = now;
            s_ec1_scope_combo_lock = 0u;
        }
        if (ec2_down != 0u && s_ec2_btn_prev == 0u) {
            s_ec2_scope_press_tick = now;
            s_ec2_scope_combo_lock = 0u;
        }
        if (ec1_down != 0u && step2 != 0) {
            s_ec1_scope_combo_lock = 1u;
        }
        if (ec2_down != 0u && step1 != 0) {
            s_ec2_scope_combo_lock = 1u;
        }
    }

    /* EC1 按键：波形页短按=RUN/STOP，长按=V/div；其它页=确认 */
    {
        if (s_screen == UI_SCR_SCOPE) {
            if (ec1_down == 0u && s_ec1_btn_prev != 0u) {
                TickType_t dur = now - s_ec1_scope_press_tick;
                if (dur >= pdMS_TO_TICKS(EC_SCOPE_LONG_MS)) {
                    if (s_ec1_scope_combo_lock == 0u) {
                        UI_Waveform_CycleVoltPerDiv();
                        s_redraw_req = 1u;
                    }
                } else if (dur >= pdMS_TO_TICKS(20u)) {
                    if ((now - s_ec1_last_ms) >= pdMS_TO_TICKS(BTN_COOLDOWN_MS)) {
                        UI_Waveform_ToggleRunStop();
                        s_redraw_req   = 1u;
                        s_ec1_last_ms = now;
                    }
                }
            }
        } else if (ec1_down != 0u && s_ec1_btn_prev == 0u) {
            if ((now - s_ec1_last_ms) >= pdMS_TO_TICKS(BTN_COOLDOWN_MS)) {
                nav_apply_ok();
                s_redraw_req   = 1u;
                s_ec1_last_ms = now;
            }
        }
        s_ec1_btn_prev = ec1_down;
    }

    /* EC2 按键：波形页短按=返回，长按=ms/div；其它页下降沿=返回 */
    {
        if (s_screen == UI_SCR_SCOPE) {
            if (ec2_down == 0u && s_ec2_btn_prev != 0u) {
                TickType_t dur = now - s_ec2_scope_press_tick;
                if (dur >= pdMS_TO_TICKS(EC_SCOPE_LONG_MS)) {
                    if (s_ec2_scope_combo_lock == 0u) {
                        UI_Waveform_CycleTimePerDiv();
                        s_redraw_req = 1u;
                    }
                } else if (dur >= pdMS_TO_TICKS(20u)) {
                    if ((now - s_ec2_last_ms) >= pdMS_TO_TICKS(BTN_COOLDOWN_MS)) {
                        nav_apply_back();
                        s_redraw_req   = 1u;
                        s_ec2_last_ms = now;
                    }
                }
            }
        } else if (ec2_down != 0u && s_ec2_btn_prev == 0u) {
            if ((now - s_ec2_last_ms) >= pdMS_TO_TICKS(BTN_COOLDOWN_MS)) {
                nav_apply_back();
                s_redraw_req   = 1u;
                s_ec2_last_ms = now;
            }
        }
        s_ec2_btn_prev = ec2_down;
    }
}

void UI_Nav_OnKey(UI_NavKey_t key)
{
    s_redraw_req = 1u;

    switch (key) {
    case UI_NAV_KEY_UP:
        if (s_screen == UI_SCR_FPGA_CONFIG) {
            if (s_fpga_mode_focus > 0) {
                s_fpga_mode_focus--;
            }
        } else if (s_menu_focus > 0) {
            s_menu_focus--;
        }
        break;
    case UI_NAV_KEY_DOWN:
        if (s_screen == UI_SCR_FPGA_CONFIG) {
            if (s_fpga_mode_focus < 2) {
                s_fpga_mode_focus++;
            }
        } else if (s_menu_focus < 6) {
            s_menu_focus++;
        }
        break;
    case UI_NAV_KEY_OK:
        nav_apply_ok();
        break;
    case UI_NAV_KEY_BACK:
        nav_apply_back();
        break;
    default:
        break;
    }
}
