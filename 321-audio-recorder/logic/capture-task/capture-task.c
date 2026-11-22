#include "capture-task/capture-task.h"

bool capture_task_start(uint32_t count, uint32_t period_us)
{
    return platform_capture_start(count, period_us);
}

bool capture_task_done(void)
{
    return platform_capture_done();
}

void capture_task_get(platform_capture_t *capture)
{
    platform_capture_get(capture);
}
