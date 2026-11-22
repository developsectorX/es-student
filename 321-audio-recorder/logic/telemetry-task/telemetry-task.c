#include "telemetry-task/telemetry-task.h"

#include <stddef.h>

#include "platform/platform.h"
#include "systime/systime.h"

static telemetry_state_t telemetry_state = TELEMETRY_STATE_IDLE;
static uint32_t telemetry_period_ms = 100u;
static uint64_t telemetry_last_us = 0u;
static telemetry_task_callback_t telemetry_on_record = NULL;

void telemetry_task_init(telemetry_task_callback_t on_record)
{
    telemetry_state = TELEMETRY_STATE_IDLE;
    telemetry_period_ms = 100u;
    telemetry_last_us = 0u;
    telemetry_on_record = on_record;
}

bool telemetry_task_start(uint32_t period_ms)
{
    if (period_ms < 10u)
    {
        return false;
    }
    telemetry_period_ms = period_ms;
    telemetry_last_us = systime_us();
    telemetry_state = TELEMETRY_STATE_RUN;
    return true;
}

void telemetry_task_stop(void)
{
    telemetry_state = TELEMETRY_STATE_IDLE;
}

telemetry_state_t telemetry_task_get_state(void)
{
    return telemetry_state;
}

uint32_t telemetry_task_get_period_ms(void)
{
    return telemetry_period_ms;
}

void telemetry_task_handle(void)
{
    if (telemetry_state != TELEMETRY_STATE_RUN)
    {
        return;
    }

    if (systime_period_elapsed(&telemetry_last_us, (uint64_t)telemetry_period_ms * 1000u))
    {
        telemetry_record_t record;
        record.time_us = systime_us();
        record.voltage_v = platform_voltage_read();
        record.temp_c = platform_temp_read();
        if (telemetry_on_record != NULL)
        {
            telemetry_on_record(&record);
        }
    }
}

bool telemetry_task_channel_busy(void)
{
    return platform_adc_busy();
}

float telemetry_task_read_voltage(void)
{
    return platform_voltage_read();
}

float telemetry_task_read_temp(void)
{
    return platform_temp_read();
}
