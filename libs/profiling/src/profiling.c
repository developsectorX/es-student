#include "profiling/profiling.h"

#include <stddef.h>

#if PROFILING_ENABLED
#include "pico/stdlib.h"

typedef struct
{
    const char *name;
    bool active;
    bool running;
    uint64_t start_us;
    uint32_t count;
    uint32_t sum_us;
    uint32_t max_us;
} profiling_stopwatch_t;

static profiling_stopwatch_t stopwatches[PROFILING_MAX_STOPWATCHES];

void profiling_stopwatch_init(uint32_t id, const char *name)
{
    if (id >= PROFILING_MAX_STOPWATCHES)
    {
        return;
    }

    stopwatches[id].name = name;
    stopwatches[id].active = true;
    stopwatches[id].running = false;
    stopwatches[id].start_us = 0;
    stopwatches[id].count = 0;
    stopwatches[id].sum_us = 0;
    stopwatches[id].max_us = 0;
}

void profiling_start(uint32_t id)
{
    if (id >= PROFILING_MAX_STOPWATCHES || !stopwatches[id].active)
    {
        return;
    }
    stopwatches[id].start_us = time_us_64();
    stopwatches[id].running = true;
}

void profiling_stop(uint32_t id)
{
    if (id >= PROFILING_MAX_STOPWATCHES || !stopwatches[id].active || !stopwatches[id].running)
    {
        return;
    }

    uint64_t now = time_us_64();
    uint32_t delta = (uint32_t)(now - stopwatches[id].start_us);
    stopwatches[id].running = false;
    stopwatches[id].count++;
    stopwatches[id].sum_us += delta;
    if (delta > stopwatches[id].max_us)
    {
        stopwatches[id].max_us = delta;
    }
}

bool profiling_get(uint32_t id, profiling_result_t *result)
{
    if (id >= PROFILING_MAX_STOPWATCHES || result == NULL || !stopwatches[id].active)
    {
        return false;
    }

    result->name = stopwatches[id].name;
    result->count = stopwatches[id].count;
    result->max_us = stopwatches[id].max_us;
    if (stopwatches[id].count > 0)
    {
        result->mean_us = (double)stopwatches[id].sum_us / (double)stopwatches[id].count;
    }
    else
    {
        result->mean_us = 0.0;
    }
    return true;
}

void profiling_reset(void)
{
    for (uint32_t i = 0; i < PROFILING_MAX_STOPWATCHES; i++)
    {
        stopwatches[i].count = 0;
        stopwatches[i].sum_us = 0;
        stopwatches[i].max_us = 0;
        stopwatches[i].running = false;
    }
}
#else
typedef struct
{
    const char *name;
    bool active;
} profiling_stopwatch_stub_t;

static profiling_stopwatch_stub_t stopwatches[PROFILING_MAX_STOPWATCHES];

void profiling_stopwatch_init(uint32_t id, const char *name)
{
    if (id >= PROFILING_MAX_STOPWATCHES)
    {
        return;
    }
    stopwatches[id].name = name;
    stopwatches[id].active = true;
}

bool profiling_get(uint32_t id, profiling_result_t *result)
{
    if (id >= PROFILING_MAX_STOPWATCHES || result == NULL || !stopwatches[id].active)
    {
        return false;
    }
    result->name = stopwatches[id].name;
    result->count = 0;
    result->mean_us = 0.0;
    result->max_us = 0;
    return true;
}

void profiling_reset(void)
{
}
#endif
