#include "button-task/button-task.h"

#include <stddef.h>

#include "systime/systime.h"

#define BUTTON_DEBOUNCE_US 20000u
#define BUTTON_LONG_PRESS_US 1000000u

typedef enum
{
    BUTTON_STATE_RELEASED,
    BUTTON_STATE_PRESS_DEBOUNCE,
    BUTTON_STATE_PRESSED,
    BUTTON_STATE_RELEASE_DEBOUNCE,
} button_state_t;

typedef struct
{
    button_state_t state;
    uint64_t state_time_us;
    uint64_t press_time_us;
    bool long_reported;
    uint32_t press_count;
    button_task_callback_t on_press;
    button_task_callback_t on_long_press;
} button_ctx_t;

static button_ctx_t buttons[PLATFORM_BUTTON_COUNT];

void button_task_init(platform_button_t button,
                      button_task_callback_t on_press,
                      button_task_callback_t on_long_press)
{
    if (button >= PLATFORM_BUTTON_COUNT)
    {
        return;
    }

    buttons[button].state = BUTTON_STATE_RELEASED;
    buttons[button].state_time_us = 0u;
    buttons[button].press_time_us = 0u;
    buttons[button].long_reported = false;
    buttons[button].press_count = 0u;
    buttons[button].on_press = on_press;
    buttons[button].on_long_press = on_long_press;
}

static void button_task_handle_one(platform_button_t id)
{
    button_ctx_t *b = &buttons[id];
    bool level = platform_button_read(id);
    uint64_t now = systime_us();
    bool has_long = (b->on_long_press != NULL);

    switch (b->state)
    {
    case BUTTON_STATE_RELEASED:
        if (level)
        {
            b->state = BUTTON_STATE_PRESS_DEBOUNCE;
            b->state_time_us = now;
        }
        break;

    case BUTTON_STATE_PRESS_DEBOUNCE:
        if (!level)
        {
            b->state = BUTTON_STATE_RELEASED;
        }
        else if ((now - b->state_time_us) >= BUTTON_DEBOUNCE_US)
        {
            b->state = BUTTON_STATE_PRESSED;
            b->press_time_us = now;
            b->long_reported = false;
            b->press_count++;
            // если у кнопки нет долгого действия, сообщаем о нажатии сразу
            if (!has_long && b->on_press != NULL)
            {
                b->on_press();
            }
        }
        break;

    case BUTTON_STATE_PRESSED:
        if (!level)
        {
            b->state = BUTTON_STATE_RELEASE_DEBOUNCE;
            b->state_time_us = now;
        }
        else if (has_long && !b->long_reported && (now - b->press_time_us) >= BUTTON_LONG_PRESS_US)
        {
            b->long_reported = true;
            b->on_long_press();
        }
        break;

    case BUTTON_STATE_RELEASE_DEBOUNCE:
        if (level)
        {
            b->state = BUTTON_STATE_PRESSED;
        }
        else if ((now - b->state_time_us) >= BUTTON_DEBOUNCE_US)
        {
            b->state = BUTTON_STATE_RELEASED;
            // короткое нажатие с долгим действием сообщается при отпускании
            if (has_long && !b->long_reported && b->on_press != NULL)
            {
                b->on_press();
            }
        }
        break;

    default:
        b->state = BUTTON_STATE_RELEASED;
        break;
    }
}

void button_task_handle(void)
{
    for (uint32_t i = 0; i < (uint32_t)PLATFORM_BUTTON_COUNT; i++)
    {
        button_task_handle_one((platform_button_t)i);
    }
}

bool button_task_is_pressed(platform_button_t button)
{
    if (button >= PLATFORM_BUTTON_COUNT)
    {
        return false;
    }
    return buttons[button].state == BUTTON_STATE_PRESSED || buttons[button].state == BUTTON_STATE_RELEASE_DEBOUNCE;
}

uint32_t button_task_get_press_count(platform_button_t button)
{
    if (button >= PLATFORM_BUTTON_COUNT)
    {
        return 0u;
    }
    return buttons[button].press_count;
}
