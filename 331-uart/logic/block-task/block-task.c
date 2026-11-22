#include "block-task/block-task.h"

#include <stddef.h>

#include "platform/platform.h"

static bool active = false;
static block_task_callback_t callback = NULL;

bool block_task_start(uint32_t count, block_task_callback_t on_block)
{
    if (active)
    {
        return false;
    }

    if (!platform_block_start(count))
    {
        return false;
    }

    callback = on_block;
    active = true;
    return true;
}

void block_task_handle(void)
{
    if (!active || !platform_block_ready())
    {
        return;
    }

    block_task_callback_t done = callback;
    const uint16_t *samples = platform_block_samples();
    uint32_t count = platform_block_count();
    uint32_t time_us = platform_block_time_us();

    active = false;
    callback = NULL;

    if (done != NULL)
    {
        done(samples, count, time_us);
    }
}
