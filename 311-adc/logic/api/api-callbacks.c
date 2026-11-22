#include "api/api-commands.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "api/api.h"
#include "button-task/button-task.h"
#include "device/device.h"
#include "firmware.h"
#include "led-task/led-task.h"
#include "pi-task/pi-task.h"
#include "platform/platform.h"
#include "profiling/profiling.h"
#include "systime/systime.h"
#include "telemetry-task/telemetry-task.h"

// период телеметрии по умолчанию
#define TELEMETRY_DEFAULT_PERIOD_MS 100u

static bool parse_u32(const char *text, uint32_t *out)
{
    if (text == NULL || out == NULL || *text == '\0')
    {
        return false;
    }

    errno = 0;
    char *end = NULL;
    unsigned long value = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0')
    {
        return false;
    }
    if (value > 0xFFFFFFFFul)
    {
        return false;
    }

    *out = (uint32_t)value;
    return true;
}

static void print_led_state(void)
{
    led_state_t state = led_task_get_state();
    const char *name = "off";
    if (state == LED_STATE_ON)
    {
        name = "on";
    }
    else if (state == LED_STATE_BLINK)
    {
        name = "blink";
    }

    printf("led: %s, period %lu ms\r\n", name, (unsigned long)led_task_get_period_ms());
}

static void cmd_info(const command_t *command)
{
    (void)command;
    device_info_t info;
    device_get_info(&info);

    printf("name: %s\r\n", FIRMWARE_NAME);
    printf("version: %s\r\n", FIRMWARE_VERSION);
    printf("project: %s\r\n", FIRMWARE_PROJECT);
    printf("repo: %s\r\n", FIRMWARE_REPO);
    printf("board: %s\r\n", info.board);
    printf("serial: %s\r\n", info.serial);
    printf("chip: manufacturer 0x%03x, part 0x%04x, revision %lu\r\n",
           (unsigned)info.chip_manufacturer,
           (unsigned)info.chip_part,
           (unsigned long)info.chip_revision);
    printf("pico-sdk: %s\r\n", info.sdk_version);
}

static void cmd_uptime(const command_t *command)
{
    (void)command;
    printf("uptime: %lu ms\r\n", (unsigned long)systime_ms());
}

static void cmd_led_enable(const command_t *command)
{
    (void)command;
    led_task_set_state(LED_STATE_ON);
    print_led_state();
}

static void cmd_led_disable(const command_t *command)
{
    (void)command;
    led_task_set_state(LED_STATE_OFF);
    print_led_state();
}

static void cmd_led_blink(const command_t *command)
{
    (void)command;
    led_task_set_state(LED_STATE_BLINK);
    print_led_state();
}

static void cmd_led_period(const command_t *command)
{
    if (command->argc != 1)
    {
        printf("error: usage led_period <period_ms>\r\n");
        return;
    }

    uint32_t period_ms = 0u;
    if (!parse_u32(command->argv[0], &period_ms))
    {
        printf("error: usage led_period <period_ms>\r\n");
        return;
    }
    if (period_ms == 0u)
    {
        printf("error: period_ms must be greater than 0\r\n");
        return;
    }

    led_task_set_period_ms(period_ms);
    print_led_state();
}

static void cmd_button(const command_t *command)
{
    (void)command;
    printf("button: %s, presses %lu\r\n",
           button_task_is_pressed() ? "pressed" : "released",
           (unsigned long)button_task_get_press_count());
}

static void cmd_pi_start(const command_t *command)
{
    uint32_t terms = 1000000u;

    if (command->argc > 1)
    {
        printf("error: usage pi_start [<terms>]\r\n");
        return;
    }

    if (command->argc == 1)
    {
        if (!parse_u32(command->argv[0], &terms))
        {
            printf("error: usage pi_start [<terms>]\r\n");
            return;
        }
        if (terms == 0u)
        {
            printf("error: terms must be greater than 0\r\n");
            return;
        }
    }

    if (pi_task_is_running())
    {
        printf("error: pi is busy\r\n");
        return;
    }

    if (!pi_task_start(terms))
    {
        printf("error: pi is busy\r\n");
        return;
    }

    printf("pi: started, %lu terms\r\n", (unsigned long)terms);
}

static void cmd_pi(const command_t *command)
{
    (void)command;

    if (pi_task_is_running())
    {
        printf("pi: running\r\n");
        return;
    }

    if (!pi_task_was_started())
    {
        printf("pi: not started\r\n");
        return;
    }

    pi_result_t result;
    if (pi_task_get_result(&result))
    {
        printf("pi: %.9f in %lu ms\r\n", result.value, (unsigned long)result.time_ms);
    }
    else
    {
        printf("pi: running\r\n");
    }
}

static void cmd_profiling(const command_t *command)
{
    (void)command;

#if PROFILING_ENABLED
    bool any = false;
    for (uint32_t i = 0; i < PROFILING_MAX_STOPWATCHES; i++)
    {
        profiling_result_t result;
        if (profiling_get(i, &result))
        {
            any = true;
            printf("%-10s count %-10lu mean %.2f us  max %lu us\r\n",
                   result.name ? result.name : "",
                   (unsigned long)result.count,
                   result.mean_us,
                   (unsigned long)result.max_us);
        }
    }
    if (!any)
    {
        /* нет заведённых секундомеров — ничего не выводим */
    }
#else
    printf("profiling is disabled in this build\r\n");
#endif
}

static void cmd_profiling_reset(const command_t *command)
{
    (void)command;
#if PROFILING_ENABLED
    profiling_reset();
    printf("profiling reset\r\n");
#else
    printf("profiling is disabled in this build\r\n");
#endif
}

// напряжение на GP26 в platform, ответ voltage: <напряжение> V
static void cmd_get_voltage(const command_t *command)
{
    (void)command;
    printf("voltage: %.3f V\r\n", platform_voltage_read());
}

// температура в platform, ответ temp: <температура> C
static void cmd_get_temp(const command_t *command)
{
    (void)command;
    printf("temp: %.1f C\r\n", platform_temp_read());
}

void api_print_telemetry(const telemetry_record_t *record)
{
    if (record == NULL)
    {
        return;
    }
    printf("tm: %llu %.3f %.1f\r\n",
           (unsigned long long)record->time_us,
           record->voltage_v,
           record->temp_c);
}

static void cmd_tm_start(const command_t *command)
{
    uint32_t period_ms = TELEMETRY_DEFAULT_PERIOD_MS;

    if (command->argc > 1)
    {
        printf("error: usage tm_start [<period_ms>]\r\n");
        return;
    }

    if (command->argc == 1)
    {
        if (!parse_u32(command->argv[0], &period_ms))
        {
            printf("error: usage tm_start [<period_ms>]\r\n");
            return;
        }
        if (period_ms < 10u)
        {
            printf("error: period_ms must be at least 10\r\n");
            return;
        }
    }

    if (telemetry_task_start(period_ms))
    {
        printf("telemetry: started, period %lu ms\r\n", (unsigned long)telemetry_task_get_period_ms());
    }
    else
    {
        printf("error: period_ms must be at least 10\r\n");
    }
}

static void cmd_tm_stop(const command_t *command)
{
    (void)command;
    telemetry_task_stop();
    printf("telemetry: stopped\r\n");
}

// charge_on: замыкаем цепь, ответ charge: on
static void cmd_charge_on(const command_t *command)
{
    (void)command;
    platform_charge_set(true);
    printf("charge: on\r\n");
}

// charge_off: размыкаем цепь, ответ charge: off
static void cmd_charge_off(const command_t *command)
{
    (void)command;
    platform_charge_set(false);
    printf("charge: off\r\n");
}

const api_command_t api_commands[] = {
    {"info", "device passport", cmd_info},
    {"uptime", "time since reset", cmd_uptime},
    {"led_enable", "turn led on", cmd_led_enable},
    {"led_disable", "turn led off", cmd_led_disable},
    {"led_blink", "blink led", cmd_led_blink},
    {"led_period", "set led period: led_period <period_ms>", cmd_led_period},
    {"button", "button state and press count", cmd_button},
    {"pi_start", "start pi calculation: pi_start [<terms>]", cmd_pi_start},
    {"pi", "pi result or state", cmd_pi},
    {"profiling", "stopwatch results", cmd_profiling},
    {"profiling_reset", "reset stopwatch results", cmd_profiling_reset},
    {"get_voltage", "voltage on GP26, ADC input 0", cmd_get_voltage},
    {"get_temp", "chip temperature, degrees Celsius", cmd_get_temp},
    {"tm_start", "start telemetry: tm_start [<period_ms>]", cmd_tm_start},
    {"tm_stop", "stop telemetry", cmd_tm_stop},
    {"charge_on", "charge RC circuit through GP22", cmd_charge_on},
    {"charge_off", "discharge RC circuit through GP22", cmd_charge_off},
};

const uint32_t api_command_count = (uint32_t)(sizeof(api_commands) / sizeof(api_commands[0]));
