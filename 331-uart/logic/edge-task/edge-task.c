#include "edge-task/edge-task.h"

#include "systime/systime.h"

#define EMPTY_V 0.05f
#define RISE_V 2.0f
#define FULL_V 3.2f
#define FALL_V 1.3f
#define TIMEOUT_US 2000000u

typedef enum
{
    EDGE_STATE_IDLE,
    EDGE_STATE_DISCHARGE,
    EDGE_STATE_RISE,
    EDGE_STATE_CHARGE,
    EDGE_STATE_FALL,
} edge_state_t;

static edge_state_t state = EDGE_STATE_IDLE;
static platform_line_mode_t line_mode = PLATFORM_LINE_PUSH_PULL;
static edge_task_callback_t callback = NULL;
static uint64_t phase_start_us = 0u;
static uint32_t rise_us = 0u;

static void enter(edge_state_t next, bool high)
{
    platform_line_set(line_mode, high);
    phase_start_us = systime_us();
    state = next;
}

static void finish(bool ok, uint32_t rise, uint32_t fall)
{
    edge_task_callback_t done = callback;
    platform_line_mode_t mode = line_mode;

    state = EDGE_STATE_IDLE;
    callback = NULL;

    if (done != NULL)
    {
        done(mode, ok, rise, fall);
    }
}

bool edge_task_start(platform_line_mode_t mode, edge_task_callback_t on_done)
{
    if (state != EDGE_STATE_IDLE)
    {
        return false;
    }

    line_mode = mode;
    callback = on_done;
    rise_us = 0u;
    enter(EDGE_STATE_DISCHARGE, false);
    return true;
}

void edge_task_handle(void)
{
    if (state == EDGE_STATE_IDLE)
    {
        return;
    }

    uint64_t now = systime_us();
    uint32_t elapsed_us = (uint32_t)(now - phase_start_us);
    float v = platform_voltage_read();

    if (elapsed_us > TIMEOUT_US)
    {
        finish(false, 0u, 0u);
        return;
    }

    switch (state)
    {
    case EDGE_STATE_DISCHARGE:
        if (v < EMPTY_V)
        {
            enter(EDGE_STATE_RISE, true);
        }
        break;
    case EDGE_STATE_RISE:
        if (v > RISE_V)
        {
            rise_us = elapsed_us;
            enter(EDGE_STATE_CHARGE, true);
        }
        break;
    case EDGE_STATE_CHARGE:
        if (v > FULL_V)
        {
            enter(EDGE_STATE_FALL, false);
        }
        break;
    case EDGE_STATE_FALL:
        if (v < FALL_V)
        {
            finish(true, rise_us, elapsed_us);
        }
        break;
    case EDGE_STATE_IDLE:
    default:
        break;
    }
}
