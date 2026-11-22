#include "stream-task/stream-task.h"

#include <stddef.h>

#include "platform/platform.h"

static stream_task_callback_t stream_on_frame = NULL;
static stream_state_t stream_state = STREAM_STATE_IDLE;
static uint8_t stream_bits = 16u;
static uint16_t stream_frame_number = 0u;
static uint32_t stream_frames = 0u;
static uint32_t stream_samples_total = 0u;

void stream_task_init(stream_task_callback_t on_frame)
{
    stream_on_frame = on_frame;
    stream_state = STREAM_STATE_IDLE;
    stream_bits = 16u;
    stream_frame_number = 0u;
    stream_frames = 0u;
    stream_samples_total = 0u;
}

bool stream_task_start(uint8_t bits)
{
    if (stream_state == STREAM_STATE_RUN)
    {
        return false;
    }

    if (!platform_stream_start(bits))
    {
        return false;
    }

    stream_bits = bits;
    stream_frame_number = 0u;
    stream_frames = 0u;
    stream_samples_total = 0u;
    stream_state = STREAM_STATE_RUN;
    return true;
}

void stream_task_stop(void)
{
    if (stream_state != STREAM_STATE_RUN)
    {
        return;
    }

    platform_stream_stop();
    stream_state = STREAM_STATE_IDLE;
}

void stream_task_handle(void)
{
    if (stream_state != STREAM_STATE_RUN)
    {
        return;
    }

    uint32_t samples = 0u;
    const uint8_t *buffer = platform_stream_next(&samples);
    if (buffer == NULL)
    {
        return;
    }

    stream_header_t header;
    header.sync_word = (uint16_t)STREAM_SYNC_WORD;
    header.number = stream_frame_number;
    header.count = (uint16_t)samples;

    uint32_t bytes = samples * ((stream_bits == 8u) ? 1u : 2u);
    if (stream_on_frame != NULL)
    {
        stream_on_frame(&header, buffer, bytes);
    }

    platform_stream_release();
    stream_frame_number = (uint16_t)(stream_frame_number + 1u);
    stream_frames++;
    stream_samples_total += samples;
}

stream_state_t stream_task_get_state(void)
{
    return stream_state;
}

uint8_t stream_task_get_bits(void)
{
    return stream_bits;
}

uint32_t stream_task_get_frames(void)
{
    return stream_frames;
}

uint32_t stream_task_get_samples(void)
{
    return stream_samples_total;
}

uint32_t stream_task_get_overruns(void)
{
    return platform_stream_overruns();
}
