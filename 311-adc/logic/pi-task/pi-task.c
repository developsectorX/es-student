#include "pi-task/pi-task.h"

#include <stddef.h>

#include "systime/systime.h"

// ~585 тактов на член ряда, при 125 МГц ~4.7 мкс/член.
// 100 членов ~470 мкс < 0.5 мс, поэтому обработчик укладывается в Н1.
#define PI_TERMS_PER_CALL 100u

typedef enum
{
    PI_TASK_STATE_NOT_STARTED,
    PI_TASK_STATE_RUNNING,
    PI_TASK_STATE_DONE,
} pi_task_state_t;

static pi_task_state_t pi_state = PI_TASK_STATE_NOT_STARTED;
static uint32_t pi_total_terms = 0u;
static uint32_t pi_k = 0u;
static double pi_acc = 0.0;
static double pi_value = 0.0;
static uint64_t pi_start_us = 0u;
static uint64_t pi_time_ms = 0u;

bool pi_task_start(uint32_t terms)
{
    if (terms == 0u)
    {
        return false;
    }
    if (pi_state == PI_TASK_STATE_RUNNING)
    {
        return false;
    }

    pi_state = PI_TASK_STATE_RUNNING;
    pi_total_terms = terms;
    pi_k = 0u;
    pi_acc = 0.0;
    pi_start_us = systime_us();
    pi_time_ms = 0u;
    return true;
}

void pi_task_handle(void)
{
    if (pi_state != PI_TASK_STATE_RUNNING)
    {
        return;
    }

    uint32_t begin = pi_k;
    uint32_t end = begin + PI_TERMS_PER_CALL;

    // защита от переполнения при больших pi_total_terms (до UINT32_MAX)
    if (end < begin || end > pi_total_terms)
    {
        end = pi_total_terms;
    }

    for (uint32_t k = begin; k < end; k++)
    {
        double term = 1.0 / (2.0 * (double)k + 1.0);
        if ((k & 1u) == 0u)
        {
            pi_acc += term;
        }
        else
        {
            pi_acc -= term;
        }
    }

    pi_k = end;

    if (pi_k >= pi_total_terms)
    {
        pi_value = pi_acc * 4.0;
        pi_time_ms = (systime_us() - pi_start_us) / 1000u;
        pi_state = PI_TASK_STATE_DONE;
    }
}

bool pi_task_is_running(void)
{
    return pi_state == PI_TASK_STATE_RUNNING;
}

bool pi_task_is_done(void)
{
    return pi_state == PI_TASK_STATE_DONE;
}

bool pi_task_was_started(void)
{
    return pi_state != PI_TASK_STATE_NOT_STARTED;
}

bool pi_task_get_result(pi_result_t *result)
{
    if (result == NULL || pi_state != PI_TASK_STATE_DONE)
    {
        return false;
    }
    result->value = pi_value;
    result->time_ms = pi_time_ms;
    return true;
}

uint32_t pi_task_get_terms(void)
{
    return pi_total_terms;
}
