#include "profile_opts.h"
#include "lcd_rotate_profile.h"

#if MAINTASK_PROFILE_EMWIN_SEGMENTS

#include "FreeRTOS.h"
#include "task.h"

#if (configGENERATE_RUN_TIME_STATS != 1)
#error lcd_rotate_profile 需要 configGENERATE_RUN_TIME_STATS 与 portGET_RUN_TIME_COUNTER_VALUE
#endif

static uint32_t s_rp_t;
static uint64_t s_rp_sum_work;
static uint64_t s_rp_sum_vsync;

static uint32_t s_6a_sub_t;
static uint64_t s_6a_bucket[LR_6A_BUCKET_COUNT];

void LcdRotateProfile_ManualRotate_Enter(void)
{
    uint32_t t = portGET_RUN_TIME_COUNTER_VALUE();
    s_rp_t     = t;
    s_6a_sub_t = t;
}

void LcdRotateProfile_6a_SubTick(unsigned idx)
{
    unsigned i = idx;
    if (i >= LR_6A_BUCKET_COUNT)
        i = LR_6A_BUCKET_COUNT - 1U;
    {
        uint32_t n = portGET_RUN_TIME_COUNTER_VALUE();
        s_6a_bucket[i] += (uint64_t)(uint32_t)(n - s_6a_sub_t);
        s_6a_sub_t = n;
    }
}

void LcdRotateProfile_BeforeVsyncWait(void)
{
    uint32_t n = portGET_RUN_TIME_COUNTER_VALUE();
    s_rp_sum_work += (uint64_t)(uint32_t)(n - s_rp_t);
    s_rp_t = n;
}

void LcdRotateProfile_AfterVsyncWait(void)
{
    uint32_t n = portGET_RUN_TIME_COUNTER_VALUE();
    s_rp_sum_vsync += (uint64_t)(uint32_t)(n - s_rp_t);
}

void LcdRotateProfile_TakeRotateSplit(uint32_t frame_count,
    uint64_t *out_work_ticks, uint64_t *out_vsync_ticks,
    uint64_t *out_6a_buckets)
{
    unsigned j;
    (void)frame_count;
    if (out_work_ticks)
        *out_work_ticks = s_rp_sum_work;
    if (out_vsync_ticks)
        *out_vsync_ticks = s_rp_sum_vsync;
    s_rp_sum_work  = 0;
    s_rp_sum_vsync = 0;

    if (out_6a_buckets) {
        for (j = 0U; j < LR_6A_BUCKET_COUNT; j++) {
            out_6a_buckets[j] = s_6a_bucket[j];
            s_6a_bucket[j]    = 0;
        }
    }
}

#else /* !MAINTASK_PROFILE_EMWIN_SEGMENTS */

void LcdRotateProfile_ManualRotate_Enter(void) { }
void LcdRotateProfile_BeforeVsyncWait(void) { }
void LcdRotateProfile_AfterVsyncWait(void) { }

void LcdRotateProfile_6a_SubTick(unsigned idx) { (void)idx; }

void LcdRotateProfile_TakeRotateSplit(uint32_t frame_count,
    uint64_t *out_work_ticks, uint64_t *out_vsync_ticks,
    uint64_t *out_6a_buckets)
{
    unsigned j;
    (void)frame_count;
    if (out_work_ticks)
        *out_work_ticks = 0;
    if (out_vsync_ticks)
        *out_vsync_ticks = 0;
    if (out_6a_buckets) {
        for (j = 0U; j < LR_6A_BUCKET_COUNT; j++)
            out_6a_buckets[j] = 0;
    }
}

#endif /* MAINTASK_PROFILE_EMWIN_SEGMENTS */
