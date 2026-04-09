#ifndef LCD_ROTATE_PROFILE_H
#define LCD_ROTATE_PROFILE_H

#include <stdint.h>

#define LR_6A_BUCKET_COUNT  8U

void LcdRotateProfile_ManualRotate_Enter(void);
/* 6a 子阶段：从上一次 tick（或 Enter）到本调用之间的 tick 记入 bucket[idx] */
void LcdRotateProfile_6a_SubTick(unsigned idx);
void LcdRotateProfile_BeforeVsyncWait(void);
void LcdRotateProfile_AfterVsyncWait(void);

/* 取出累计 tick 并清零；out_6a 可为 NULL；out_6a 为 LR_6A_BUCKET_COUNT 个 uint64 的和（未除帧数） */
void LcdRotateProfile_TakeRotateSplit(uint32_t frame_count,
    uint64_t *out_work_ticks, uint64_t *out_vsync_ticks,
    uint64_t *out_6a_buckets);

#endif /* LCD_ROTATE_PROFILE_H */
