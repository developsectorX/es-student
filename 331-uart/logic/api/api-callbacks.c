#include "api/api-commands.h"

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "block-task/block-task.h"
#include "button-task/button-task.h"
#include "device/device.h"
#include "edge-task/edge-task.h"
#include "firmware.h"
#include "led-task/led-task.h"
#include "pi-task/pi-task.h"
#include "platform/platform.h"
#include "profiling/profiling.h"
#include "systime/systime.h"
#include "uart-task/uart-task.h"

#define ECHO_MAX 32
#define BLOCK_SIZE 64
#define SCOPE_SAMPLES PLATFORM_BLOCK_MAX
#define SCOPE_BAUD_MIN 9600u
#define SCOPE_BAUD_MAX 115200u
#define SCOPE_TEXT_MAX 4

static const char text_line[] = "0123456789ABCDEFGHIJKLMNOPQRSTUV";

static uint32_t scope_baud = 115200u;
static uint8_t scope_rx[UART_TASK_MAX];
static size_t scope_rx_size = 0;
static uint32_t scope_rx_errors = 0;

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

static bool parse_i32(const char *text, int32_t *out)
{
    if (text == NULL || out == NULL || *text == '\0')
    {
        return false;
    }

    errno = 0;
    char *end = NULL;
    long value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0')
    {
        return false;
    }

    *out = (int32_t)value;
    return true;
}

static void print_bytes(const uint8_t *data, size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        unsigned char c = data[i];
        putchar(isprint(c) ? c : '.');
    }
}

static void print_errors(uint32_t errors)
{
    bool any = false;
    printf("err:");
    if (errors & PLATFORM_UART_FE)
    {
        printf(" FE");
        any = true;
    }
    if (errors & PLATFORM_UART_PE)
    {
        printf(" PE");
        any = true;
    }
    if (errors & PLATFORM_UART_BE)
    {
        printf(" BE");
        any = true;
    }
    if (errors & PLATFORM_UART_OE)
    {
        printf(" OE");
        any = true;
    }
    if (!any)
    {
        printf(" none");
    }
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


static void print_edge(platform_line_mode_t mode, bool ok, uint32_t rise_us, uint32_t fall_us)
{
    const char *name = (mode == PLATFORM_LINE_PUSH_PULL) ? "pp" : "od";
    if (!ok)
    {
        printf("edge %s: timeout\r\n", name);
        return;
    }

    printf("edge %s: rise %.1f ms, fall %.1f ms\r\n",
           name,
           (double)rise_us / 1000.0,
           (double)fall_us / 1000.0);
}

static void cmd_edge(const command_t *command)
{
    if (command->argc != 1)
    {
        printf("error: usage edge pp|od\r\n");
        return;
    }

    platform_line_mode_t mode;
    if (strcmp(command->argv[0], "pp") == 0)
    {
        mode = PLATFORM_LINE_PUSH_PULL;
    }
    else if (strcmp(command->argv[0], "od") == 0)
    {
        mode = PLATFORM_LINE_OPEN_DRAIN;
    }
    else
    {
        printf("error: usage edge pp|od\r\n");
        return;
    }

    if (!edge_task_start(mode, print_edge))
    {
        printf("error: edge is busy\r\n");
    }
}

static void cmd_loop(const command_t *command)
{
    if (command->argc != 1)
    {
        printf("error: usage loop internal|pins\r\n");
        return;
    }

    if (strcmp(command->argv[0], "internal") == 0)
    {
        platform_uart_loop_set(true);
        printf("loop: internal\r\n");
    }
    else if (strcmp(command->argv[0], "pins") == 0)
    {
        platform_uart_loop_set(false);
        printf("loop: pins\r\n");
    }
    else
    {
        printf("error: usage loop internal|pins\r\n");
    }
}

static void print_echo(const uint8_t *data, size_t size, uint32_t errors)
{
    printf("rx: ");
    print_bytes(data, size);
    printf(" ");
    print_errors(errors);
    printf("\r\n");
}

static void cmd_echo(const command_t *command)
{
    if (command->argc != 1)
    {
        printf("error: usage echo <text>\r\n");
        return;
    }

    size_t len = strlen(command->argv[0]);
    if (len > ECHO_MAX)
    {
        printf("error: text longer than 32\r\n");
        return;
    }

    if (!uart_task_start(PLATFORM_UART0, PLATFORM_UART0,
                         (const uint8_t *)command->argv[0], len, print_echo))
    {
        printf("error: uart is busy\r\n");
    }
}

static void print_link_back(const uint8_t *data, size_t size, uint32_t errors)
{
    printf("1->0: ");
    print_bytes(data, size);
    printf(" ");
    print_errors(errors);
    printf("\r\n");
}

static void print_link_forth(const uint8_t *data, size_t size, uint32_t errors)
{
    printf("0->1: ");
    print_bytes(data, size);
    printf(" ");
    print_errors(errors);
    printf("\r\n");

    if (!uart_task_start(PLATFORM_UART1, PLATFORM_UART0, data, size, print_link_back))
    {
        printf("error: uart is busy\r\n");
    }
}

static void cmd_link(const command_t *command)
{
    if (command->argc != 1)
    {
        printf("error: usage link <text>\r\n");
        return;
    }

    size_t len = strlen(command->argv[0]);
    if (len > ECHO_MAX)
    {
        printf("error: text longer than 32\r\n");
        return;
    }

    if (!uart_task_start(PLATFORM_UART0, PLATFORM_UART1,
                         (const uint8_t *)command->argv[0], len, print_link_forth))
    {
        printf("error: uart is busy\r\n");
    }
}

static void cmd_baud_shift(const command_t *command)
{
    if (command->argc != 1)
    {
        printf("error: usage baud_shift <percent>\r\n");
        return;
    }

    int32_t percent = 0;
    if (!parse_i32(command->argv[0], &percent))
    {
        printf("error: usage baud_shift <percent>\r\n");
        return;
    }

    if (percent < -50 || percent > 50)
    {
        printf("error: percent must be -50..50\r\n");
        return;
    }

    uint32_t baud = (uint32_t)((int32_t)115200 * (100 + percent) / 100);
    uint32_t actual = platform_uart_baud_set(PLATFORM_UART1, baud);
    printf("baud_shift: uart1 %lu baud\r\n", (unsigned long)actual);
}

static void print_text(const uint8_t *data, size_t size, uint32_t errors)
{
    printf("text: %lu chars ", (unsigned long)size);
    print_bytes(data, size);
    printf(" ");
    print_errors(errors);
    printf("\r\n");
}

static void cmd_text(const command_t *command)
{
    (void)command;
    if (command->argc != 0)
    {
        printf("error: usage text\r\n");
        return;
    }

    if (!uart_task_start(PLATFORM_UART1, PLATFORM_UART0,
                         (const uint8_t *)text_line, strlen(text_line), print_text))
    {
        printf("error: uart is busy\r\n");
    }
}

static bool parse_parity(const char *text, platform_parity_t *out)
{
    if (strcmp(text, "none") == 0)
    {
        *out = PLATFORM_PARITY_NONE;
        return true;
    }
    if (strcmp(text, "even") == 0)
    {
        *out = PLATFORM_PARITY_EVEN;
        return true;
    }
    if (strcmp(text, "odd") == 0)
    {
        *out = PLATFORM_PARITY_ODD;
        return true;
    }
    return false;
}

static const char *parity_name(platform_parity_t parity)
{
    if (parity == PLATFORM_PARITY_EVEN)
    {
        return "even";
    }
    if (parity == PLATFORM_PARITY_ODD)
    {
        return "odd";
    }
    return "none";
}

static void cmd_parity(const command_t *command)
{
    if (command->argc != 2)
    {
        printf("error: usage parity none|even|odd none|even|odd\r\n");
        return;
    }

    platform_parity_t rx_parity;
    platform_parity_t tx_parity;
    if (!parse_parity(command->argv[0], &rx_parity) || !parse_parity(command->argv[1], &tx_parity))
    {
        printf("error: usage parity none|even|odd none|even|odd\r\n");
        return;
    }

    platform_uart_parity_set(PLATFORM_UART0, rx_parity);
    platform_uart_parity_set(PLATFORM_UART1, tx_parity);
    printf("parity: uart0 %s, uart1 %s\r\n", parity_name(rx_parity), parity_name(tx_parity));
}

static const char *block_mode_name = "irq";

static void print_block(const uint8_t *data, size_t size, uint32_t errors)
{
    platform_uart_rx_irq_set(false);

    bool match = (size > 0);
    if (size > BLOCK_SIZE)
    {
        size = BLOCK_SIZE;
    }
    for (size_t i = 0; i < size; i++)
    {
        if (data[i] != (uint8_t)i)
        {
            match = false;
            break;
        }
    }

    printf("block %s: %lu/%u %s, ",
           block_mode_name,
           (unsigned long)size,
           (unsigned)BLOCK_SIZE,
           match ? "match" : "mismatch");
    print_errors(errors);
    printf("\r\n");
}

static void cmd_block(const command_t *command)
{
    if (command->argc != 1)
    {
        printf("error: usage block irq|poll\r\n");
        return;
    }

    bool irq_mode;
    if (strcmp(command->argv[0], "irq") == 0)
    {
        irq_mode = true;
        block_mode_name = "irq";
    }
    else if (strcmp(command->argv[0], "poll") == 0)
    {
        irq_mode = false;
        block_mode_name = "poll";
    }
    else
    {
        printf("error: usage block irq|poll\r\n");
        return;
    }

    uint8_t block[BLOCK_SIZE];
    for (uint32_t i = 0; i < BLOCK_SIZE; i++)
    {
        block[i] = (uint8_t)i;
    }

    platform_uart_rx_irq_set(irq_mode);

    if (!uart_task_start(PLATFORM_UART1, PLATFORM_UART0, block, sizeof(block), print_block))
    {
        platform_uart_rx_irq_set(false);
        printf("error: uart is busy\r\n");
    }
}

static void print_scope_rx(const uint8_t *data, size_t size, uint32_t errors)
{
    if (size > sizeof(scope_rx))
    {
        size = sizeof(scope_rx);
    }
    memcpy(scope_rx, data, size);
    scope_rx_size = size;
    scope_rx_errors = errors;
}

static void print_scope(const uint16_t *samples, uint32_t count, uint32_t time_us)
{
    (void)time_us;

    // вернуть оба модуля на исходную скорость после захвата
    platform_uart_baud_set(PLATFORM_UART0, 115200);
    platform_uart_baud_set(PLATFORM_UART1, 115200);

    printf("scope: %lu samples, %lu Hz, %lu baud, uart1 ",
           (unsigned long)count,
           (unsigned long)PLATFORM_SAMPLE_RATE_HZ,
           (unsigned long)scope_baud);
    print_bytes(scope_rx, scope_rx_size);
    printf(" ");
    print_errors(scope_rx_errors);
    printf("\r\n");

    for (uint32_t i = 0; i < count; i++)
    {
        printf("%u", (unsigned)samples[i]);
        if (((i + 1u) % 16u) == 0u || (i + 1u) == count)
        {
            printf("\r\n");
        }
        else
        {
            printf(" ");
        }
    }

    printf("scope: end\r\n");
}

static void cmd_scope(const command_t *command)
{
    if (command->argc != 2)
    {
        printf("error: usage scope <baud> <text>\r\n");
        return;
    }

    uint32_t baud = 0u;
    if (!parse_u32(command->argv[0], &baud))
    {
        printf("error: usage scope <baud> <text>\r\n");
        return;
    }
    if (baud < SCOPE_BAUD_MIN || baud > SCOPE_BAUD_MAX)
    {
        printf("error: usage scope <baud> <text>\r\n");
        return;
    }

    size_t len = strlen(command->argv[1]);
    if (len == 0u || len > SCOPE_TEXT_MAX)
    {
        printf("error: usage scope <baud> <text>\r\n");
        return;
    }

    scope_baud = baud;
    scope_rx_size = 0;
    scope_rx_errors = 0;

    platform_uart_baud_set(PLATFORM_UART0, baud);
    platform_uart_baud_set(PLATFORM_UART1, baud);

    if (!block_task_start(SCOPE_SAMPLES, print_scope))
    {
        platform_uart_baud_set(PLATFORM_UART0, 115200);
        platform_uart_baud_set(PLATFORM_UART1, 115200);
        printf("error: adc busy\r\n");
        return;
    }

    if (!uart_task_start(PLATFORM_UART0, PLATFORM_UART1,
                         (const uint8_t *)command->argv[1], len, print_scope_rx))
    {
        platform_uart_baud_set(PLATFORM_UART0, 115200);
        platform_uart_baud_set(PLATFORM_UART1, 115200);
        printf("error: uart is busy\r\n");
        return;
    }
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
    {"edge", "rise and fall of the RC line: edge pp|od", cmd_edge},
    {"loop", "UART0 loopback: loop internal|pins", cmd_loop},
    {"echo", "text through UART0 and back: echo <text>", cmd_echo},
    {"link", "text from UART0 to UART1 and back: link <text>", cmd_link},
    {"baud_shift", "shift UART1 baud rate: baud_shift <percent>", cmd_baud_shift},
    {"text", "32 characters from UART1 to UART0", cmd_text},
    {"parity", "parity of UART0 and UART1: parity <rx> <tx>", cmd_parity},
    {"block", "64 bytes from UART1 to UART0: block irq|poll", cmd_block},
    {"scope", "UART0 line on ADC: scope <baud> <text>", cmd_scope},
};

const uint32_t api_command_count = (uint32_t)(sizeof(api_commands) / sizeof(api_commands[0]));
