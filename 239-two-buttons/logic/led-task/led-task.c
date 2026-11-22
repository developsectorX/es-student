#include "led-task/led-task.h"

#include <stdbool.h>

#include "platform/platform.h"
#include "systime/systime.h"

static led_state_t led_state = LED_STATE_OFF;
static uint32_t led_period_ms = 1000u;
static uint64_t led_last_toggle_us = 0u;
static bool led_output_on = false;

static const uint32_t led_period_cycle[] = {250u, 500u, 1000u};

static void led_task_apply_output(bool on)
{
    led_output_on = on;
    platform_led_set(on);
}

void led_task_init(void)
{
    led_period_ms = 1000u;
    led_task_set_state(LED_STATE_BLINK);
}

void led_task_set_state(led_state_t state)
{
    led_state = state;

    // действие при входе
    if (state == LED_STATE_OFF)
    {
        led_task_apply_output(false);
    }
    else if (state == LED_STATE_ON)
    {
        led_task_apply_output(true);
    }
    else /* LED_STATE_BLINK */
    {
        led_task_apply_output(true);
        // первое переключение должно случиться через полпериода
        led_last_toggle_us = systime_us();
    }
}

led_state_t led_task_get_state(void)
{
    return led_state;
}

void led_task_set_period_ms(uint32_t period_ms)
{
    if (period_ms == 0u)
    {
        return;
    }
    led_period_ms = period_ms;
}

uint32_t led_task_get_period_ms(void)
{
    return led_period_ms;
}

void led_task_next_state(void)
{
    if (led_state == LED_STATE_OFF)
    {
        led_task_set_state(LED_STATE_ON);
    }
    else if (led_state == LED_STATE_ON)
    {
        led_task_set_state(LED_STATE_BLINK);
    }
    else
    {
        led_task_set_state(LED_STATE_OFF);
    }
}

void led_task_next_period(void)
{
    // круг периодов 250 -> 500 -> 1000 мс
    uint32_t next = led_period_cycle[0];
    for (uint32_t i = 0; i < (uint32_t)(sizeof(led_period_cycle) / sizeof(led_period_cycle[0])); i++)
    {
        if (led_period_ms == led_period_cycle[i])
        {
            uint32_t ni = (i + 1u) % (uint32_t)(sizeof(led_period_cycle) / sizeof(led_period_cycle[0]));
            next = led_period_cycle[ni];
            led_task_set_period_ms(next);
            return;
        }
    }
    led_task_set_period_ms(next);
}

void led_task_blink(void)
{
    led_task_set_state(LED_STATE_BLINK);
}

void led_task_handle(void)
{
    switch (led_state)
    {
    case LED_STATE_OFF:
    case LED_STATE_ON:
        // светодиод установлен при входе
        break;
    case LED_STATE_BLINK:
        if (systime_period_elapsed(&led_last_toggle_us, ((uint64_t)led_period_ms * 1000u) / 2u))
        {
            led_task_apply_output(!led_output_on);
        }
        break;
    default:
        break;
    }
}
