#include "GUI.h"
#include "ui_screens.h"
#include "ui_theme.h"
#include "ui_nav.h"
#include "ui_app_config.h"
#include "FPGAConfigDefaultTask.h"
#include <stdio.h>

static void ui_text_prepare(void)
{
    GUI_SetTextMode(GUI_TM_TRANS);
}

/* 避免 GUI_DispStringHCenterAt 在旋转后/字宽取整上与 MainTask 手写坐标不一致 */
static void disp_str_cx(const char *s, int cx, int y)
{
    int w;
    ui_text_prepare();
    w = GUI_GetStringDistX(s);
    GUI_DispStringAt(s, cx - (w / 2), y);
}

static void fill_bg(void)
{
    GUI_SetColor(UI_CLR_BG);
    GUI_FillRect(0, 0, UI_DISP_W - 1, UI_DISP_H - 1);
    ui_text_prepare();
}

/* 与 MainTask DrawHeader 相同的顶栏分层与分割线，仅替换标题与右上文案 */
static void draw_header_bar(const char *title, const char *right_tag)
{
    ui_text_prepare();

    GUI_SetColor(UI_CLR_HDR_TOP);
    GUI_FillRect(0, 0, UI_DISP_W - 1, 33);
    GUI_SetColor(UI_CLR_HDR_BOT);
    GUI_FillRect(0, 34, UI_DISP_W - 1, UI_HDR_H - 1);

    GUI_SetColor(UI_CLR_DIV);
    GUI_DrawHLine(33, 0, UI_DISP_W - 1);
    GUI_DrawHLine(UI_HDR_H, 0, UI_DISP_W - 1);

    GUI_SetFont(&GUI_Font16_ASCII);
    GUI_SetColor(UI_CLR_TEXT_HI);
    disp_str_cx(title, UI_DISP_W / 2, 9);

    if (right_tag != NULL && right_tag[0] != '\0') {
        GUI_SetFont(&GUI_Font13_ASCII);
        GUI_SetColor(UI_CLR_TEXT_MID);
        {
            int tw = GUI_GetStringDistX(right_tag);
            GUI_DispStringAt(right_tag, UI_DISP_W - 12 - tw, 15);
        }
    }
}

static void draw_footer_bar(const char *hint)
{
    int y0 = UI_DISP_H - UI_FOOTER_H;
    GUI_SetColor(UI_CLR_HDR_BOT);
    GUI_FillRect(0, y0, UI_DISP_W - 1, UI_DISP_H - 1);
    GUI_SetColor(UI_CLR_DIV);
    GUI_DrawHLine(y0, 0, UI_DISP_W - 1);
    GUI_SetFont(&GUI_Font8_ASCII);
    GUI_SetColor(UI_CLR_TEXT_DIM);
    ui_text_prepare();
    GUI_DispStringAt(hint, UI_MARGIN_X, y0 + 5);
}

/* 圆角半径与 MainTask 徽章一致用 4，避免过圆角在低密度屏上发糊 */
static void draw_menu_card(int x, int y, int w, int h, int selected, const char *line1, const char *line2)
{
    if (selected) {
        GUI_SetColor(UI_CLR_SEL);
        GUI_FillRoundedRect(x, y, x + w - 1, y + h - 1, 4);
        GUI_SetColor(UI_CLR_ACCENT);
        GUI_DrawRoundedRect(x, y, x + w - 1, y + h - 1, 4);
        GUI_SetColor(UI_CLR_TEXT_HI);
    } else {
        GUI_SetColor(UI_CLR_BG_PLOT);
        GUI_FillRoundedRect(x, y, x + w - 1, y + h - 1, 4);
        GUI_SetColor(UI_CLR_CARD_BR);
        GUI_DrawRoundedRect(x, y, x + w - 1, y + h - 1, 4);
        GUI_SetColor(UI_CLR_TEXT_MID);
    }

    ui_text_prepare();
    GUI_SetFont(&GUI_Font16_ASCII);
    GUI_DispStringAt(line1, x + 10, y + 8);
    if (line2 != NULL && line2[0] != '\0') {
        GUI_SetFont(&GUI_Font8_ASCII);
        GUI_SetColor(selected ? UI_CLR_TEXT_HI : UI_CLR_TEXT_DIM);
        GUI_DispStringAt(line2, x + 10, y + 30);
    }
}

static const char *fpga_mode_title(FPGA_UI_Mode_t mode)
{
    switch (mode) {
    case FPGA_UI_MODE_SLAVE_SERIAL:
        return "SLAVE SERIAL";
    case FPGA_UI_MODE_JTAG_SRAM:
        return "JTAG-SRAM";
    case FPGA_UI_MODE_JTAG_FLASH:
        return "JTAG-FLASH";
    default:
        return "FPGA CONFIG";
    }
}

static const char *fpga_mode_short_name(FPGA_UI_Mode_t mode)
{
    switch (mode) {
    case FPGA_UI_MODE_SLAVE_SERIAL:
        return "Slave Serial";
    case FPGA_UI_MODE_JTAG_SRAM:
        return "JTAG-SRAM";
    case FPGA_UI_MODE_JTAG_FLASH:
        return "JTAG-Flash";
    default:
        return "---";
    }
}

static const char *ui_wait_dots(uint32_t tick_ms)
{
    static const char *const dots[] = {"", ".", "..", "..."};
    return dots[(tick_ms / 350U) & 3U];
}

static void draw_fpga_status_panel(int x0, int y0, int x1, int y1)
{
    GUI_SetColor(UI_CLR_BG_PLOT);
    GUI_FillRoundedRect(x0, y0, x1, y1, 6);
    GUI_SetColor(UI_CLR_ACCENT);
    GUI_DrawRoundedRect(x0, y0, x1, y1, 6);
}

static void screen_welcome(uint32_t tick_ms)
{
    const int cx = UI_DISP_W / 2;

    (void)tick_ms;
    fill_bg();

    GUI_SetFont(&GUI_Font16_ASCII);
    GUI_SetColor(UI_CLR_TEXT_HI);
    disp_str_cx("DUAL CHANNEL OSCILLOSCOPE", cx, UI_DISP_H / 2 - 68);

    GUI_SetFont(&GUI_Font13_ASCII);
    GUI_SetColor(UI_CLR_CH1);
    disp_str_cx("WELCOME", cx, UI_DISP_H / 2 - 32);

    GUI_SetColor(UI_CLR_TEXT_MID);
    disp_str_cx("STM32H743  @400MHz", cx, UI_DISP_H / 2 + 4);

    GUI_SetFont(&GUI_Font8_ASCII);
    GUI_SetColor(UI_CLR_TEXT_DIM);
    disp_str_cx("UI demo  |  emWin", cx, UI_DISP_H / 2 + 34);
}

static void screen_main_menu(uint32_t tick_ms)
{
    static const char *const menu_title[] = {
        "Oscilloscope",
        "Channel setup",
        "Trigger",
        "Auto measure",
        "System",
        "FPGA Configuration",
        "Remote Control"
    };
    static const char *const menu_subtitle[] = {
        "Waveform display",
        "V/div  coupling  probe",
        "Edge  mode  level  coupling",
        "All params CH1 / CH2",
        "Brightness  language  ver.",
        "Slave Serial  JTAG-SRAM  JTAG-Flash",
        "1920x1080 preview  cursor mapping"
    };
    const int item_count = (int)(sizeof(menu_title) / sizeof(menu_title[0]));
    const int card_h     = 54;
    const int gap        = 7;
    const int step       = card_h + gap;
    const int x          = UI_MARGIN_X;
    const int w          = UI_DISP_W - UI_MARGIN_X - UI_MARGIN_XR;
    const int list_y0    = UI_CONTENT_TOP + 6;
    const int list_y1    = UI_DISP_H - UI_FOOTER_H - 8;
    const int view_h     = list_y1 - list_y0 + 1;
    const int content_h  = item_count * card_h + (item_count - 1) * gap;
    const int max_scroll = (content_h > view_h) ? (content_h - view_h) : 0;
    int sel              = UI_Nav_GetMenuFocus();
    int scroll           = 0;
    int y;
    int i;
    GUI_RECT clip_rect;
    const GUI_RECT *old_clip;

    (void)tick_ms;
    if (sel < 0) {
        sel = 0;
    }
    if (sel > (item_count - 1)) {
        sel = item_count - 1;
    }
    if (max_scroll > 0 && item_count > 1) {
        scroll = (sel * max_scroll) / (item_count - 1);
    }

    fill_bg();
    draw_header_bar("MAIN MENU", "DEMO");
#if UI_DEMO_AUTO_ADVANCE
    draw_footer_bar("EC1: focus/OK  |  EC2: BACK  |  scope: turn=move  hold+turn=solo/mode  btn=RUN/V/ret/T");
#else
    draw_footer_bar("EC1: focus/OK  |  EC2: BACK  |  scope: turn=move  hold+turn=solo/mode  btn=RUN/V/ret/T");
#endif

    clip_rect.x0 = x;
    clip_rect.y0 = list_y0;
    clip_rect.x1 = x + w - 1;
    clip_rect.y1 = list_y1;
    old_clip = GUI_SetClipRect(&clip_rect);

    y = list_y0 - scroll;
    for (i = 0; i < item_count; ++i) {
        draw_menu_card(x, y, w, card_h, sel == i, menu_title[i], menu_subtitle[i]);
        y += step;
    }

    GUI_SetClipRect(old_clip);

    if (max_scroll > 0) {
        const int track_x = UI_DISP_W - 11;
        int thumb_h = (view_h * view_h) / content_h;
        int thumb_y = list_y0;

        if (thumb_h < 28) {
            thumb_h = 28;
        }
        thumb_y += ((view_h - thumb_h) * scroll) / max_scroll;

        GUI_SetColor(UI_CLR_DIV);
        GUI_DrawVLine(track_x, list_y0, list_y1);
        GUI_SetColor(UI_CLR_ACCENT);
        GUI_FillRoundedRect(track_x - 2, thumb_y, track_x + 2, thumb_y + thumb_h - 1, 2);
    }
}

static void ch_row(int x_name, int x_val, int y, const char *name, const char *value)
{
    ui_text_prepare();
    GUI_SetFont(&GUI_Font13_ASCII);
    GUI_SetColor(UI_CLR_TEXT_MID);
    GUI_DispStringAt(name, x_name, y);
    GUI_SetColor(UI_CLR_TEXT_HI);
    GUI_DispStringAt(value, x_val, y);
}

static void screen_channel(uint32_t tick_ms)
{
    /* 左 CH1 / 右 CH2 对称；垂直档位、耦合、探头衰减、通道开关 */
    const int y0  = UI_CONTENT_TOP + 44;
    const int dy  = 30;
    const int x1n = UI_MARGIN_X + 8;
    const int x1v = 210;
    const int x2n = UI_DISP_W / 2 + 18;
    const int x2v = UI_DISP_W / 2 + 222;
    const int mid = UI_DISP_W / 2;

    (void)tick_ms;
    fill_bg();
    draw_header_bar("CHANNEL SETUP", "CH1 | CH2");
    draw_footer_bar("BACK: main menu  |  EC: change value / focus");

    GUI_SetColor(UI_CLR_DIV);
    GUI_DrawVLine(mid, UI_CONTENT_TOP + 38, UI_DISP_H - UI_FOOTER_H - 14);

    GUI_SetColor(UI_CLR_CH1_BADGE);
    GUI_FillRoundedRect(16, UI_CONTENT_TOP + 6, mid - 10, UI_CONTENT_TOP + 32, 4);
    GUI_SetColor(UI_CLR_CH1);
    GUI_DrawRoundedRect(16, UI_CONTENT_TOP + 6, mid - 10, UI_CONTENT_TOP + 32, 4);
    GUI_SetFont(&GUI_Font13_ASCII);
    GUI_SetColor(UI_CLR_CH1);
    ui_text_prepare();
    GUI_DispStringAt("CH1", 22, UI_CONTENT_TOP + 14);

    GUI_SetColor(UI_CLR_BG_PLOT);
    GUI_FillRoundedRect(16, UI_CONTENT_TOP + 38, mid - 10, UI_DISP_H - UI_FOOTER_H - 12, 4);
    GUI_SetColor(UI_CLR_ACCENT);
    GUI_DrawRoundedRect(16, UI_CONTENT_TOP + 38, mid - 10, UI_DISP_H - UI_FOOTER_H - 12, 4);
    ch_row(x1n, x1v, y0 + 0 * dy, "V/div", "500 mV");
    ch_row(x1n, x1v, y0 + 1 * dy, "Coupling", "DC");
    ch_row(x1n, x1v, y0 + 2 * dy, "Probe", "x1");
    ch_row(x1n, x1v, y0 + 3 * dy, "Channel", "ON");

    GUI_SetColor(UI_CLR_CH2_BADGE);
    GUI_FillRoundedRect(mid + 10, UI_CONTENT_TOP + 6, UI_DISP_W - 16, UI_CONTENT_TOP + 32, 4);
    GUI_SetColor(UI_CLR_CH2);
    GUI_DrawRoundedRect(mid + 10, UI_CONTENT_TOP + 6, UI_DISP_W - 16, UI_CONTENT_TOP + 32, 4);
    GUI_SetColor(UI_CLR_CH2);
    GUI_DispStringAt("CH2", mid + 16, UI_CONTENT_TOP + 14);

    GUI_SetColor(UI_CLR_BG_PLOT);
    GUI_FillRoundedRect(mid + 10, UI_CONTENT_TOP + 38, UI_DISP_W - 16, UI_DISP_H - UI_FOOTER_H - 12, 4);
    GUI_SetColor(UI_CLR_ACCENT);
    GUI_DrawRoundedRect(mid + 10, UI_CONTENT_TOP + 38, UI_DISP_W - 16, UI_DISP_H - UI_FOOTER_H - 12, 4);
    ch_row(x2n, x2v, y0 + 0 * dy, "V/div", "1.00 V");
    ch_row(x2n, x2v, y0 + 1 * dy, "Coupling", "DC");
    ch_row(x2n, x2v, y0 + 2 * dy, "Probe", "x10");
    ch_row(x2n, x2v, y0 + 3 * dy, "Channel", "ON");

    GUI_SetFont(&GUI_Font8_ASCII);
    GUI_SetColor(UI_CLR_TEXT_DIM);
    ui_text_prepare();
    GUI_DispStringAt("DC / AC / GND", x1n, y0 + 4 * dy);
    GUI_DispStringAt("DC / AC / GND", x2n, y0 + 4 * dy);
}

static void screen_trigger(uint32_t tick_ms)
{
    /* 触发沿、模式、电平、耦合(含噪声抑制)；列表式 */
    const int y0   = UI_CONTENT_TOP + 16;
    const int dy   = 34;
    const int xn   = UI_MARGIN_X + 14;
    const int xv   = 260;
    int y;

    (void)tick_ms;
    fill_bg();
    draw_header_bar("TRIGGER", "EDGE");
    draw_footer_bar("BACK: return  |  source: CH1 (demo)");

    GUI_SetColor(UI_CLR_BG_PLOT);
    GUI_FillRoundedRect(UI_MARGIN_X, y0 - 6, UI_DISP_W - UI_MARGIN_XR, UI_DISP_H - UI_FOOTER_H - 10, 4);
    GUI_SetColor(UI_CLR_ACCENT);
    GUI_DrawRoundedRect(UI_MARGIN_X, y0 - 6, UI_DISP_W - UI_MARGIN_XR, UI_DISP_H - UI_FOOTER_H - 10, 4);

    GUI_SetFont(&GUI_Font13_ASCII);
    y = y0;
    ch_row(xn, xv, y, "Edge slope", "Rising");
    y += dy;
    ch_row(xn, xv, y, "Alt: falling", "OFF");
    y += dy;
    ch_row(xn, xv, y, "Mode", "Auto");
    y += dy;
    ch_row(xn, xv, y, "Alt: Normal / Single", "---");
    y += dy;
    ch_row(xn, xv, y, "Level", "1.20 V");
    y += dy;
    ch_row(xn, xv, y, "Coupling", "DC");
    y += dy;
    ch_row(xn, xv, y, "Alt: AC / Noise rej.", "---");
    /* 上一行 Font13 约 15px 高，勿用 y+8 写提示，否则会与末行重叠 */
    y += 22;

    GUI_SetFont(&GUI_Font8_ASCII);
    GUI_SetColor(UI_CLR_TEXT_DIM);
    ui_text_prepare();
    GUI_DispStringAt("Modes: Auto Normal Single | Coupl.: DC AC Noise rej.", UI_MARGIN_X + 10, y);
}

static void meas_side_row(int xn, int xv, int y, const char *name, const char *val, GUI_COLOR clr_val)
{
    GUI_SetFont(&GUI_Font13_ASCII);
    GUI_SetColor(UI_CLR_TEXT_MID);
    ui_text_prepare();
    GUI_DispStringAt(name, xn, y);
    GUI_SetColor(clr_val);
    GUI_DispStringAt(val, xv, y);
}

static void screen_measure(uint32_t tick_ms)
{
    /* 左 CH1 / 右 CH2 对称列表：频率、周期、峰峰值、最大最小、占空比、上升时间 */
    static const char *names[] = {
        "Frequency", "Period", "Vpp", "Vmax", "Vmin", "Duty", "Rise time"
    };
    static const char *v1[] = {
        "1.002 kHz", "998.0 us", "2.96 V", "+1.48 V", "-1.48 V", "50.0 %", "320 ns"
    };
    static const char *v2[] = {
        "500.0 Hz", "2.000 ms", "1.85 V", "+0.92 V", "-0.90 V", "49.2 %", "410 ns"
    };
    const int n     = 7;
    int i;
    const int ytop  = UI_CONTENT_TOP + 40;
    const int dy    = 28;
    const int mid   = UI_DISP_W / 2;
    const int x1n   = UI_MARGIN_X + 10;
    const int x1v   = 200;
    const int x2n   = mid + 14;
    const int x2v   = mid + 204;

    (void)tick_ms;
    fill_bg();
    draw_header_bar("AUTO MEASURE", "CH1 | CH2");
    draw_footer_bar("Demo values  |  one-screen overview");

    GUI_SetColor(UI_CLR_DIV);
    GUI_DrawVLine(mid, UI_CONTENT_TOP + 6, UI_DISP_H - UI_FOOTER_H - 12);

    GUI_SetColor(UI_CLR_CH1_BADGE);
    GUI_FillRoundedRect(16, UI_CONTENT_TOP + 6, mid - 10, UI_CONTENT_TOP + 30, 4);
    GUI_SetColor(UI_CLR_CH1);
    GUI_DrawRoundedRect(16, UI_CONTENT_TOP + 6, mid - 10, UI_CONTENT_TOP + 30, 4);
    GUI_SetFont(&GUI_Font13_ASCII);
    ui_text_prepare();
    GUI_DispStringAt("CH1", 24, UI_CONTENT_TOP + 12);

    GUI_SetColor(UI_CLR_CH2_BADGE);
    GUI_FillRoundedRect(mid + 10, UI_CONTENT_TOP + 6, UI_DISP_W - 16, UI_CONTENT_TOP + 30, 4);
    GUI_SetColor(UI_CLR_CH2);
    GUI_DrawRoundedRect(mid + 10, UI_CONTENT_TOP + 6, UI_DISP_W - 16, UI_CONTENT_TOP + 30, 4);
    GUI_DispStringAt("CH2", mid + 18, UI_CONTENT_TOP + 12);

    for (i = 0; i < n; i++) {
        int y = ytop + i * dy;
        meas_side_row(x1n, x1v, y, names[i], v1[i], UI_CLR_CH1);
        meas_side_row(x2n, x2v, y, names[i], v2[i], UI_CLR_CH2);
    }
}

static void screen_system(uint32_t tick_ms)
{
    /* 亮度、背光时间、语言、恢复默认、版本（导航由 EC 编码器接 UI_Nav_OnKey） */
    const int y0 = UI_CONTENT_TOP + 10;
    const int dy = 30;
    const int xn = UI_MARGIN_X + 14;
    const int xv = 280;
    int y;

    (void)tick_ms;
    fill_bg();
    draw_header_bar("SYSTEM SETUP", "EC nav");
    draw_footer_bar("EC: select / adjust  |  BACK: main menu");

    GUI_SetColor(UI_CLR_BG_PLOT);
    GUI_FillRoundedRect(UI_MARGIN_X, y0 - 4, UI_DISP_W - UI_MARGIN_XR, UI_DISP_H - UI_FOOTER_H - 10, 4);
    GUI_SetColor(UI_CLR_ACCENT);
    GUI_DrawRoundedRect(UI_MARGIN_X, y0 - 4, UI_DISP_W - UI_MARGIN_XR, UI_DISP_H - UI_FOOTER_H - 10, 4);

    y = y0;
    GUI_SetFont(&GUI_Font16_ASCII);
    GUI_SetColor(UI_CLR_TEXT_HI);
    ui_text_prepare();
    GUI_DispStringAt("Display && power", xn, y);
    y += 26;
    GUI_SetFont(&GUI_Font13_ASCII);
    ch_row(xn, xv, y, "Brightness", "72 %");
    y += dy;
    ch_row(xn, xv, y, "Backlight timeout", "5 min");
    y += dy;
    ch_row(xn, xv, y, "Alt: Always on", "---");
    y += dy + 8;

    GUI_SetFont(&GUI_Font16_ASCII);
    GUI_SetColor(UI_CLR_TEXT_HI);
    GUI_DispStringAt("Regional", xn, y);
    y += 26;
    GUI_SetFont(&GUI_Font13_ASCII);
    ch_row(xn, xv, y, "Language", "English");
    y += dy;
    ch_row(xn, xv, y, "Alt: Chinese (UTF8 UI TBD)", "CN");

    y += dy + 8;
    GUI_SetFont(&GUI_Font16_ASCII);
    GUI_SetColor(UI_CLR_TEXT_HI);
    GUI_DispStringAt("Maintenance", xn, y);
    y += 26;
    GUI_SetFont(&GUI_Font13_ASCII);
    GUI_SetColor(UI_CLR_TEXT_MID);
    ui_text_prepare();
    GUI_DispStringAt("Restore factory defaults", xn, y);
    GUI_SetColor(UI_CLR_ACCENT);
    GUI_DispStringAt(">", xv, y);

    y += dy + 10;
    GUI_SetColor(UI_CLR_DIV);
    GUI_DrawHLine(y, UI_MARGIN_X + 8, UI_DISP_W - UI_MARGIN_XR - 8);
    y += 14;
    GUI_SetFont(&GUI_Font13_ASCII);
    GUI_SetColor(UI_CLR_TEXT_MID);
    ui_text_prepare();
    GUI_DispStringAt("Firmware", xn, y);
    GUI_SetColor(UI_CLR_TEXT_HI);
    GUI_DispStringAt("UI v1.0  (demo)", xv, y);
    y += dy - 4;
    GUI_SetColor(UI_CLR_TEXT_MID);
    GUI_DispStringAt("Build", xn, y);
    GUI_SetColor(UI_CLR_TEXT_HI);
    GUI_DispStringAt("v7_h743  emWin", xv, y);
}

static void screen_fpga_config(uint32_t tick_ms)
{
    const int card_h = 74;
    const int gap    = 12;
    const int x      = UI_MARGIN_X;
    const int w      = UI_DISP_W - UI_MARGIN_X - UI_MARGIN_XR;
    const int sel    = UI_Nav_GetFpgaModeFocus();
    int y            = UI_CONTENT_TOP + 34;

    (void)tick_ms;
    fill_bg();
    draw_header_bar("FPGA CONFIGURATION", "");
    draw_footer_bar("EC1: select / enter  |  EC2: back");

    GUI_SetFont(&GUI_Font13_ASCII);
    GUI_SetColor(UI_CLR_TEXT_DIM);
    ui_text_prepare();
    GUI_DispStringAt("Available configuration modes", UI_MARGIN_X, UI_CONTENT_TOP + 8);

    draw_menu_card(x, y, w, card_h, sel == 0, "Slave Serial", "Receive bin via USB, then configure FPGA");
    y += card_h + gap;
    draw_menu_card(x, y, w, card_h, sel == 1, "JTAG-SRAM", "Load bitstream to FPGA through JTAG");
    y += card_h + gap;
    draw_menu_card(x, y, w, card_h, sel == 2, "JTAG-Flash", "Program external SPI flash through JTAG");
}

static void screen_fpga_mode_detail(FPGA_UI_Mode_t mode, uint32_t tick_ms)
{
    FPGA_UI_FlowState_t flow = FPGA_UI_GetFlowState();
    const char *dots         = ui_wait_dots(tick_ms);
    const char *mode_name    = fpga_mode_short_name(mode);
    const int panel_x0       = UI_MARGIN_X;
    const int panel_y0       = UI_CONTENT_TOP + 26;
    const int panel_x1       = UI_DISP_W - UI_MARGIN_XR;
    const int panel_y1       = UI_DISP_H - UI_FOOTER_H - 12;
    char status_line[64];
    char size_line[48];

    fill_bg();
    draw_header_bar("FPGA CONFIGURATION", mode_name);

    if (flow == FPGA_UI_FLOW_BIN_DONE_WAIT_START) {
        draw_footer_bar("EC1: start configuration  |  USB: send 0x1231  |  EC2: back");
    } else if (flow == FPGA_UI_FLOW_CONFIGURING) {
        draw_footer_bar("Configuring FPGA...  |  EC2: back");
    } else if ((flow == FPGA_UI_FLOW_SUCCESS) || (flow == FPGA_UI_FLOW_FAILED)) {
        draw_footer_bar("EC2: back  |  Re-enter this mode to receive a new bin");
    } else {
        draw_footer_bar("USB: send bin + 55AAAA55  |  EC2: back");
    }

    draw_fpga_status_panel(panel_x0, panel_y0, panel_x1, panel_y1);

    GUI_SetFont(&GUI_Font16_ASCII);
    GUI_SetColor(UI_CLR_TEXT_HI);
    disp_str_cx(fpga_mode_title(mode), UI_DISP_W / 2, panel_y0 + 18);

    GUI_SetColor(UI_CLR_DIV);
    GUI_DrawHLine(panel_y0 + 48, panel_x0 + 12, panel_x1 - 12);

    GUI_SetFont(&GUI_Font13_ASCII);
    GUI_SetColor(UI_CLR_TEXT_MID);
    ui_text_prepare();
    GUI_DispStringAt("Mode", panel_x0 + 18, panel_y0 + 66);
    GUI_SetColor(UI_CLR_TEXT_HI);
    GUI_DispStringAt(mode_name, panel_x0 + 110, panel_y0 + 66);

    GUI_SetColor(UI_CLR_TEXT_MID);
    GUI_DispStringAt("Bin size", panel_x0 + 18, panel_y0 + 94);
    GUI_SetColor(UI_CLR_TEXT_HI);
    snprintf(size_line, sizeof(size_line), "%lu bytes", (unsigned long)FPGA_UI_GetBinSize());
    GUI_DispStringAt(size_line, panel_x0 + 110, panel_y0 + 94);

    GUI_SetColor(UI_CLR_DIV);
    GUI_DrawHLine(panel_y0 + 126, panel_x0 + 12, panel_x1 - 12);

    GUI_SetFont(&GUI_Font16_ASCII);
    GUI_SetColor(UI_CLR_TEXT_HI);
    if (flow == FPGA_UI_FLOW_BIN_DONE_WAIT_START) {
        snprintf(status_line, sizeof(status_line), "Waiting start command%s", dots);
        disp_str_cx("Bin file receive done!", UI_DISP_W / 2, panel_y0 + 154);
        disp_str_cx(status_line, UI_DISP_W / 2, panel_y0 + 186);
        GUI_SetFont(&GUI_Font13_ASCII);
        GUI_SetColor(UI_CLR_TEXT_MID);
        disp_str_cx("(Press EC1 to start configuration)", UI_DISP_W / 2, panel_y0 + 226);
    } else if (flow == FPGA_UI_FLOW_CONFIGURING) {
        snprintf(status_line, sizeof(status_line), "Configuring FPGA%s", dots);
        disp_str_cx(status_line, UI_DISP_W / 2, panel_y0 + 170);
        GUI_SetFont(&GUI_Font13_ASCII);
        GUI_SetColor(UI_CLR_TEXT_MID);
        disp_str_cx("Please keep USB connected during configuration", UI_DISP_W / 2, panel_y0 + 210);
    } else if (flow == FPGA_UI_FLOW_SUCCESS) {
        disp_str_cx("FPGA configuration success!", UI_DISP_W / 2, panel_y0 + 170);
        GUI_SetFont(&GUI_Font13_ASCII);
        GUI_SetColor(UI_CLR_TEXT_MID);
        disp_str_cx("Press EC2 to go back", UI_DISP_W / 2, panel_y0 + 210);
    } else if (flow == FPGA_UI_FLOW_FAILED) {
        disp_str_cx("FPGA configuration failed!", UI_DISP_W / 2, panel_y0 + 170);
        GUI_SetFont(&GUI_Font13_ASCII);
        GUI_SetColor(UI_CLR_TEXT_MID);
        disp_str_cx("Press EC2 to go back and try again", UI_DISP_W / 2, panel_y0 + 210);
    } else if (flow == FPGA_UI_FLOW_WAIT_MODE) {
        disp_str_cx("Bin file receive done!", UI_DISP_W / 2, panel_y0 + 154);
        disp_str_cx("Waiting mode selection...", UI_DISP_W / 2, panel_y0 + 186);
        GUI_SetFont(&GUI_Font13_ASCII);
        GUI_SetColor(UI_CLR_TEXT_MID);
        disp_str_cx("Select mode from UI or send 0x01/0x02/0x03", UI_DISP_W / 2, panel_y0 + 226);
    } else {
        snprintf(status_line, sizeof(status_line), "Waiting bin file%s", dots);
        disp_str_cx(status_line, UI_DISP_W / 2, panel_y0 + 170);
        GUI_SetFont(&GUI_Font13_ASCII);
        GUI_SetColor(UI_CLR_TEXT_MID);
        disp_str_cx("Send bin file by USB, then append 55AAAA55", UI_DISP_W / 2, panel_y0 + 210);
    }
}

static void screen_remote_control(uint32_t tick_ms)
{
    const int frame_w   = 608;
    const int frame_h   = 342;
    const int frame_x0  = (UI_DISP_W - frame_w) / 2;
    const int frame_y0  = UI_CONTENT_TOP + 34;
    const int frame_x1  = frame_x0 + frame_w - 1;
    const int frame_y1  = frame_y0 + frame_h - 1;
    const int inner_x0  = frame_x0 + 12;
    const int inner_y0  = frame_y0 + 12;
    const int inner_x1  = frame_x1 - 12;
    const int inner_y1  = frame_y1 - 12;
    const int cursor_x  = inner_x0 + ((1000 * (inner_x1 - inner_x0)) / 1919);
    const int cursor_y  = inner_y0 + ((500  * (inner_y1 - inner_y0)) / 1079);

    (void)tick_ms;
    fill_bg();
    draw_header_bar("REMOTE CONTROL", "");
    draw_footer_bar("Static preview for future mouse coordinate mapping");

    GUI_SetFont(&GUI_Font13_ASCII);
    GUI_SetColor(UI_CLR_TEXT_DIM);
    ui_text_prepare();
    GUI_DispStringAt("Scaled 1920x1080 workspace", UI_MARGIN_X, UI_CONTENT_TOP + 8);

    GUI_SetColor(UI_CLR_BG_PLOT);
    GUI_FillRoundedRect(frame_x0, frame_y0, frame_x1, frame_y1, 6);
    GUI_SetColor(UI_CLR_ACCENT);
    GUI_DrawRoundedRect(frame_x0, frame_y0, frame_x1, frame_y1, 6);

    GUI_SetColor(UI_CLR_CARD);
    GUI_FillRect(inner_x0, inner_y0, inner_x1, inner_y1);
    GUI_SetColor(UI_CLR_CARD_BR);
    GUI_DrawRect(inner_x0, inner_y0, inner_x1, inner_y1);

    GUI_SetColor(UI_CLR_GRID_MN);
    GUI_DrawVLine(inner_x0 + ((inner_x1 - inner_x0) * 1) / 3, inner_y0, inner_y1);
    GUI_DrawVLine(inner_x0 + ((inner_x1 - inner_x0) * 2) / 3, inner_y0, inner_y1);
    GUI_DrawHLine(inner_y0 + ((inner_y1 - inner_y0) * 1) / 3, inner_x0, inner_x1);
    GUI_DrawHLine(inner_y0 + ((inner_y1 - inner_y0) * 2) / 3, inner_x0, inner_x1);

    GUI_SetColor(UI_CLR_ACCENT);
    GUI_DrawHLine(frame_y0 + 28, frame_x0 + 1, frame_x1 - 1);
    GUI_FillCircle(frame_x0 + 18, frame_y0 + 14, 3);
    GUI_SetColor(UI_CLR_TEXT_MID);
    GUI_SetFont(&GUI_Font8_ASCII);
    ui_text_prepare();
    GUI_DispStringAt("REMOTE DESKTOP", frame_x0 + 28, frame_y0 + 10);

    GUI_SetColor(UI_CLR_CH1);
    GUI_FillCircle(cursor_x, cursor_y, 6);
    GUI_SetColor(UI_CLR_TEXT_HI);
    GUI_DrawCircle(cursor_x, cursor_y, 9);

    GUI_SetColor(UI_CLR_TEXT_HI);
    GUI_SetFont(&GUI_Font13_ASCII);
    ui_text_prepare();
    GUI_DispStringAt("x:1000  y:500", inner_x0 + 10, inner_y1 - 24);
}

void UI_Screen_Draw(UI_ScreenId_t id, uint32_t tick_ms)
{
    ui_text_prepare();
    switch (id) {
    case UI_SCR_WELCOME:
        screen_welcome(tick_ms);
        break;
    case UI_SCR_MAIN_MENU:
        screen_main_menu(tick_ms);
        break;
    case UI_SCR_SCOPE:
        (void)tick_ms;
        fill_bg();
        break;
    case UI_SCR_CHANNEL:
        screen_channel(tick_ms);
        break;
    case UI_SCR_TRIGGER:
        screen_trigger(tick_ms);
        break;
    case UI_SCR_MEASURE:
        screen_measure(tick_ms);
        break;
    case UI_SCR_SYSTEM:
        screen_system(tick_ms);
        break;
    case UI_SCR_FPGA_CONFIG:
        screen_fpga_config(tick_ms);
        break;
    case UI_SCR_FPGA_SLAVE_SERIAL:
        screen_fpga_mode_detail(FPGA_UI_MODE_SLAVE_SERIAL, tick_ms);
        break;
    case UI_SCR_FPGA_JTAG_SRAM:
        screen_fpga_mode_detail(FPGA_UI_MODE_JTAG_SRAM, tick_ms);
        break;
    case UI_SCR_FPGA_JTAG_FLASH:
        screen_fpga_mode_detail(FPGA_UI_MODE_JTAG_FLASH, tick_ms);
        break;
    case UI_SCR_REMOTE_CONTROL:
        screen_remote_control(tick_ms);
        break;
    default:
        fill_bg();
        draw_header_bar("UNKNOWN", "");
        break;
    }
}
