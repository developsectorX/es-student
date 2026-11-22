#include "block-task/block-task.h"

#include <stddef.h>

#include "platform/platform.h"

static uint32_t block_count = 0u;
static block_task_callback_t block_on_block = NULL;

bool block_task_start(uint32_t count, block_task_callback_t on_block)
{
    if (block_on_block != NULL || on_block == NULL)
    {
        return false;
    }

    if (!platform_block_start(count))
    {
        return false;
    }

    block_count = count;
    block_on_block = on_block;
    return true;
}

void block_task_handle(void)
{
    if (block_on_block == NULL)
    {
        return;
    }

    if (!platform_block_done())
    {
        return;
    }

    // функцию забываем до вызова, потому что она сама может запустить следующий захват
    block_task_callback_t on_block = block_on_block;
    block_on_block = NULL;
    on_block(platform_block_samples(), block_count, platform_block_time_us());
}

bool block_task_is_running(void)
{
    return block_on_block != NULL;
}
