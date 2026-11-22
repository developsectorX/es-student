#include "systime/systime.h"

#include "pico/stdlib.h"

uint64_t systime_us(void)
{
    return time_us_64();
}

uint64_t systime_ms(void)
{
    return time_us_64() / 1000u;
}

bool systime_period_elapsed(uint64_t *last_us, uint64_t period_us)
{
    if (last_us == NULL)
    {
        return false;
    }

    uint64_t now = time_us_64();
    if (now - *last_us < period_us)
    {
        return false;
    }

    // следующее срабатывание отсчитывается от прошлого
    *last_us += period_us;

    // если отстали больше чем на период, пропуски не догоняем
    if (now - *last_us >= period_us)
    {
        *last_us = now;
    }

    return true;
}
