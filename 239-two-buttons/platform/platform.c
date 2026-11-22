#include "platform/platform.h"

#include "pico/stdlib.h"

#define PLATFORM_LED_PIN PICO_DEFAULT_LED_PIN
#define PLATFORM_BUTTON_LED_PIN 15
#define PLATFORM_BUTTON_PERIOD_PIN 14

void platform_init(void)
{
    stdio_init_all();

    gpio_init(PLATFORM_LED_PIN);
    gpio_set_dir(PLATFORM_LED_PIN, GPIO_OUT);
    gpio_put(PLATFORM_LED_PIN, 0);

    gpio_init(PLATFORM_BUTTON_LED_PIN);
    gpio_set_dir(PLATFORM_BUTTON_LED_PIN, GPIO_IN);
    gpio_pull_up(PLATFORM_BUTTON_LED_PIN);

    gpio_init(PLATFORM_BUTTON_PERIOD_PIN);
    gpio_set_dir(PLATFORM_BUTTON_PERIOD_PIN, GPIO_IN);
    gpio_pull_up(PLATFORM_BUTTON_PERIOD_PIN);
}

void platform_led_set(bool on)
{
    gpio_put(PLATFORM_LED_PIN, on ? 1 : 0);
}

bool platform_button_read(platform_button_t button)
{
    // нажатая кнопка замыкает вывод на землю и даёт ноль
    if (button == PLATFORM_BUTTON_LED)
    {
        return !gpio_get(PLATFORM_BUTTON_LED_PIN);
    }
    if (button == PLATFORM_BUTTON_PERIOD)
    {
        return !gpio_get(PLATFORM_BUTTON_PERIOD_PIN);
    }
    return false;
}
