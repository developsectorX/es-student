#include "button-task/button-task.h"

#include <stddef.h>

#include "platform/platform.h"
#include "systime/systime.h"

#define BUTTON_DEBOUNCE_US 20000u

typedef enum
{
    BUTTON_STATE_RELEASED,
    BUTTON_STATE_PRESS_DEBOUNCE,
    BUTTON_STATE_PRESSED,
    BUTTON_STATE_RELEASE_DEBOUNCE,
} button_state_t;

static button_state_t button_state = BUTTON_STATE_RELEASED;
static uint64_t button_state_time_us = 0u;
static uint32_t button_press_count = 0u;
static button_task_callback_t button_on_press = NULL;

void button_task_init(button_task_callback_t on_press)
{
    button_state = BUTTON_STATE_RELEASED;
    button_state_time_us = 0u;
    button_press_count = 0u;
    button_on_press = on_press;
}

void button_task_handle(void)
{
    bool level = platform_button_read();
    uint64_t now = systime_us();

    switch (button_state)
    {
    case BUTTON_STATE_RELEASED:
        if (level)
        {
            button_state = BUTTON_STATE_PRESS_DEBOUNCE;
            button_state_time_us = now;
        }
        break;

    case BUTTON_STATE_PRESS_DEBOUNCE:
        if (!level)
        {
            button_state = BUTTON_STATE_RELEASED;
        }
        else if ((now - button_state_time_us) >= BUTTON_DEBOUNCE_US)
        {
            button_state = BUTTON_STATE_PRESSED;
            button_press_count++;
            if (button_on_press != NULL)
            {
                button_on_press();
            }
        }
        break;

    case BUTTON_STATE_PRESSED:
        if (!level)
        {
            button_state = BUTTON_STATE_RELEASE_DEBOUNCE;
            button_state_time_us = now;
        }
        break;

    case BUTTON_STATE_RELEASE_DEBOUNCE:
        if (level)
        {
            button_state = BUTTON_STATE_PRESSED;
        }
        else if ((now - button_state_time_us) >= BUTTON_DEBOUNCE_US)
        {
            button_state = BUTTON_STATE_RELEASED;
        }
        break;

    default:
        button_state = BUTTON_STATE_RELEASED;
        break;
    }
}

bool button_task_is_pressed(void)
{
    return button_state == BUTTON_STATE_PRESSED || button_state == BUTTON_STATE_RELEASE_DEBOUNCE;
}

uint32_t button_task_get_press_count(void)
{
    return button_press_count;
}
