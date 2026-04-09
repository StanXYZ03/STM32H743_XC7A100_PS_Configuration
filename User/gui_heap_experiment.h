/*
 * emWin GUI 堆对比实验与 SDRAM 自检开关（集中改这里即可）
 *
 * 说明：EX_SRAM=0 将 GUI 堆放在 0x24000000 与本工程 AXI SRAM 链接脚本冲突，勿用；
 *       实验仅在外扩 SDRAM（SDRAM_APP_BUF）上缩小 GUI_NUMBYTES 或做启动区测。
 */
#ifndef GUI_HEAP_EXPERIMENT_H
#define GUI_HEAP_EXPERIMENT_H

#include <stdint.h>

/* 0 = 默认 6MB；1 = 2MB；2 = 1MB（均从 SDRAM_APP_BUF 起，仅缩小 GUI_ALLOC 池） */
#ifndef GUI_HEAP_EXPERIMENT_MODE
#define GUI_HEAP_EXPERIMENT_MODE  1
#endif

#if (GUI_HEAP_EXPERIMENT_MODE == 1)
#define GUI_CONF_EXPERIMENT_NUMBYTES  (2U * 1024U * 1024U)
#elif (GUI_HEAP_EXPERIMENT_MODE == 2)
#define GUI_CONF_EXPERIMENT_NUMBYTES  (1U * 1024U * 1024U)
#else
#define GUI_CONF_EXPERIMENT_NUMBYTES  (6U * 1024U * 1024U)
#endif

/* 1 = 上电后、emWin 初始化前对 [SDRAM_APP_BUF, +GUI_CONF_EXPERIMENT_NUMBYTES) 做行走读写（0 错通过） */
#ifndef SDRAM_GUI_HEAP_TEST_AT_BOOT
#define SDRAM_GUI_HEAP_TEST_AT_BOOT  1
#endif

#endif /* GUI_HEAP_EXPERIMENT_H */
