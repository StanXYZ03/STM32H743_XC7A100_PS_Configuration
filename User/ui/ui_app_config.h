/**
 * UI 应用模式：可在包含本头文件之前 #define 覆盖默认值。
 * main.c 已包含本文件，便于在入口统一改演示开关与单屏时长。
 */
#ifndef UI_APP_CONFIG_H
#define UI_APP_CONFIG_H

#ifndef UI_APP_DEMO_CYCLE_SCREENS
#define UI_APP_DEMO_CYCLE_SCREENS  1
#endif

#ifndef UI_DEMO_SCREEN_MS
#define UI_DEMO_SCREEN_MS  10000u
#endif

/* 0=停在当前界面（EC 编码器/UI_Nav_MarkDirty 切页）；1=定时自动下一页演示 */
#ifndef UI_DEMO_AUTO_ADVANCE
#define UI_DEMO_AUTO_ADVANCE  0
#endif

/*
 * 非示波器页不重绘时的轮询周期（仅用于 UI_Nav_Poll / 等键），与刷新解耦。
 * 编码器经 I2C 轮询：过小会增加唤醒次数，过大易快转丢步；默认 8ms 约 125Hz。
 */
#ifndef UI_DEMO_IDLE_POLL_MS
#define UI_DEMO_IDLE_POLL_MS  8u
#endif

/* EC11：机械一格对应若干次正交跳变，累积满该值再移动一档（默认 4；仍偏灵敏可改为 6/8） */
#ifndef UI_ENC_PULSES_PER_DETENT
#define UI_ENC_PULSES_PER_DETENT  4
#endif

/*
 * 水平时基（ms/div）显示用「标称等效采样率」Hz：一帧 n 点对应时间 T_buf = n / Fs（秒）。
 * 默认 80000：800 点 ≈ 10ms，与 1ms/div×10 格 ≈ 10ms 对齐；接真实 ADC 后可改为与 trigger_hz 一致。
 */
#ifndef UI_SCOPE_EFFECTIVE_FS_HZ
#define UI_SCOPE_EFFECTIVE_FS_HZ  80000.0f
#endif

#endif /* UI_APP_CONFIG_H */
