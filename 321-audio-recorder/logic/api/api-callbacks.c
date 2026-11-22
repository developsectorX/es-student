#include "api/api-commands.h"

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "api/api.h"
#include "block-task/block-task.h"
#include "button-task/button-task.h"
#include "capture-task/capture-task.h"
#include "device/device.h"
#include "firmware.h"
#include "led-task/led-task.h"
#include "pi-task/pi-task.h"
#include "platform/platform.h"
#include "profiling/profiling.h"
#include "stream-task/stream-task.h"
#include "systime/systime.h"
#include "telemetry-task/telemetry-task.h"

// период телеметрии по умолчанию
#define TELEMETRY_DEFAULT_PERIOD_MS 100u

// период индикатора записи во время потока
#define STREAM_BLINK_PERIOD_MS 250u

// окно mic_level по умолчанию, мс
#define MIC_DEFAULT_WINDOW_MS 100u

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

// напряжение на GP26, ответ voltage: <напряжение> V
static void cmd_get_voltage(const command_t *command)
{
    (void)command;
    if (telemetry_task_channel_busy())
    {
        printf("error: adc busy\r\n");
        return;
    }
    printf("voltage: %.3f V\r\n", telemetry_task_read_voltage());
}

// температура кристалла, ответ temp: <температура> C
static void cmd_get_temp(const command_t *command)
{
    (void)command;
    if (telemetry_task_channel_busy())
    {
        printf("error: adc busy\r\n");
        return;
    }
    printf("temp: %.1f C\r\n", telemetry_task_read_temp());
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

    if (telemetry_task_channel_busy())
    {
        printf("error: adc busy\r\n");
        return;
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

static void cmd_timer_blink_start(const command_t *command)
{
    if (command->argc != 1)
    {
        printf("error: usage timer_blink_start <period_ms>\r\n");
        return;
    }

    uint32_t period_ms = 0u;
    if (!parse_u32(command->argv[0], &period_ms))
    {
        printf("error: usage timer_blink_start <period_ms>\r\n");
        return;
    }

    if (!platform_blink_start(period_ms))
    {
        printf("error: period_ms must be 10..60000\r\n");
        return;
    }

    // задача светодиода больше не управляет выводом
    led_task_set_state(LED_STATE_OFF);
    printf("timer_blink: started, period %lu ms\r\n", (unsigned long)period_ms);
}

static void cmd_timer_blink_stop(const command_t *command)
{
    (void)command;
    uint32_t toggles = platform_blink_stop();
    printf("timer_blink: stopped, %lu toggles\r\n", (unsigned long)toggles);
}

static void cmd_cap_start(const command_t *command)
{
    if (command->argc != 2)
    {
        printf("error: usage cap_start <N> <period_us>\r\n");
        return;
    }

    uint32_t count = 0u;
    uint32_t period_us = 0u;
    if (!parse_u32(command->argv[0], &count) || !parse_u32(command->argv[1], &period_us))
    {
        printf("error: usage cap_start <N> <period_us>\r\n");
        return;
    }

    if (count == 0u || count > PLATFORM_CAPTURE_MAX || period_us < 10u)
    {
        printf("error: N must be 1..2000, period_us at least 10\r\n");
        return;
    }

    if (telemetry_task_get_state() == TELEMETRY_STATE_RUN || !capture_task_start(count, period_us))
    {
        printf("error: adc busy\r\n");
        return;
    }

    printf("capture: started, %lu samples, period %lu us\r\n",
           (unsigned long)count, (unsigned long)period_us);
}

static void cmd_cap_dump(const command_t *command)
{
    (void)command;

    platform_capture_t capture;
    capture_task_get(&capture);
    if (capture.count == 0u)
    {
        printf("error: no capture\r\n");
        return;
    }

    printf("capture: %lu samples, period %lu us, interval %lu..%lu us, handler %.1f %%\r\n",
           (unsigned long)capture.count,
           (unsigned long)capture.period_us,
           (unsigned long)capture.min_interval_us,
           (unsigned long)capture.max_interval_us,
           capture.handler_percent);

    for (uint32_t i = 0u; i < capture.count; i += 16u)
    {
        printf("cap:");
        for (uint32_t j = 0u; j < 16u && (i + j) < capture.count; j++)
        {
            printf(" %u", (unsigned)capture.samples[i + j]);
        }
        printf("\r\n");
    }
}

static void print_dma_cap(const uint16_t *samples, uint32_t count, uint32_t time_us)
{
    uint16_t min_code = 0xFFFFu;
    uint16_t max_code = 0u;
    uint32_t sum = 0u;
    for (uint32_t i = 0u; i < count; i++)
    {
        uint16_t code = samples[i];
        if (code < min_code)
        {
            min_code = code;
        }
        if (code > max_code)
        {
            max_code = code;
        }
        sum += code;
    }

    uint64_t expected_us = ((uint64_t)count * 1000000ull) / (uint64_t)PLATFORM_SAMPLE_RATE_HZ;
    float mean = (count > 0u) ? ((float)sum / (float)count) : 0.0f;

    printf("dma_cap: %lu samples in %lu us, expected %llu us, min %u max %u mean %.1f\r\n",
           (unsigned long)count,
           (unsigned long)time_us,
           (unsigned long long)expected_us,
           (unsigned)min_code,
           (unsigned)max_code,
           mean);
}

static bool start_block(uint32_t count, block_task_callback_t on_block)
{
    if (telemetry_task_get_state() == TELEMETRY_STATE_RUN)
    {
        return false;
    }
    return block_task_start(count, on_block);
}

static void cmd_dma_cap(const command_t *command)
{
    if (command->argc != 1)
    {
        printf("error: usage dma_cap <N>\r\n");
        return;
    }

    uint32_t count = 0u;
    if (!parse_u32(command->argv[0], &count))
    {
        printf("error: usage dma_cap <N>\r\n");
        return;
    }

    if (count == 0u || count > PLATFORM_BLOCK_MAX)
    {
        printf("error: N must be 1..8192\r\n");
        return;
    }

    if (!start_block(count, print_dma_cap))
    {
        printf("error: adc busy\r\n");
        return;
    }
}

static void cmd_generate_tone(const command_t *command)
{
    if (command->argc != 1)
    {
        printf("error: usage generate_tone <Hz>\r\n");
        return;
    }

    uint32_t freq_hz = 0u;
    if (!parse_u32(command->argv[0], &freq_hz))
    {
        printf("error: usage generate_tone <Hz>\r\n");
        return;
    }

    if (!platform_tone_set(freq_hz))
    {
        printf("error: Hz must be 0 or 20..20000\r\n");
        return;
    }

    if (freq_hz == 0u)
    {
        printf("tone: off\r\n");
    }
    else
    {
        printf("tone: %lu Hz\r\n", (unsigned long)freq_hz);
    }
}

// код АЦП в вольты: 12 разрядов, опора 3,3 В
static float code_to_volts(float code)
{
    return code * 3.3f / 4096.0f;
}

// среднее и отклонение от среднего в кодах: среднеквадратичное и наибольшее
static void block_stats(const uint16_t *samples, uint32_t count, float *mean, float *rms, float *peak)
{
    uint32_t sum = 0u;
    for (uint32_t i = 0u; i < count; i++)
    {
        sum += samples[i];
    }
    int32_t center = (int32_t)(sum / count);

    // в целых числах: в float на 8192 отсчётах ядро считало бы заметно дольше
    uint64_t squares = 0u;
    uint32_t largest = 0u;
    for (uint32_t i = 0u; i < count; i++)
    {
        int32_t deviation = (int32_t)samples[i] - center;
        uint32_t size = (deviation < 0) ? (uint32_t)(-deviation) : (uint32_t)deviation;
        squares += (uint64_t)((int64_t)deviation * (int64_t)deviation);
        if (size > largest)
        {
            largest = size;
        }
    }

    *mean = (float)sum / (float)count;
    *rms = sqrtf((float)squares / (float)count);
    *peak = (float)largest;
}

static void print_mic_bias(const uint16_t *samples, uint32_t count, uint32_t time_us)
{
    (void)time_us;
    float mean = 0.0f;
    float rms = 0.0f;
    float peak = 0.0f;
    block_stats(samples, count, &mean, &rms, &peak);
    printf("mic: bias %.3f V\r\n", code_to_volts(mean));
}

static void print_mic_level(const uint16_t *samples, uint32_t count, uint32_t time_us)
{
    (void)time_us;
    float mean = 0.0f;
    float rms = 0.0f;
    float peak = 0.0f;
    block_stats(samples, count, &mean, &rms, &peak);

    float rms_mv = code_to_volts(rms) * 1000.0f;
    float peak_mv = code_to_volts(peak) * 1000.0f;
    uint32_t window_ms = (uint32_t)(((uint64_t)count * 1000ull) / (uint64_t)PLATFORM_SAMPLE_RATE_HZ);
    printf("mic: level %.1f mV rms, peak %.1f mV, %lu ms\r\n",
           rms_mv, peak_mv, (unsigned long)window_ms);
}

static void cmd_mic_bias(const command_t *command)
{
    (void)command;
    // 4410 отсчётов = 100 мс при 44100 Гц
    if (!start_block(4410u, print_mic_bias))
    {
        printf("error: adc busy\r\n");
    }
}

static void cmd_mic_level(const command_t *command)
{
    uint32_t window_ms = MIC_DEFAULT_WINDOW_MS;

    if (command->argc > 1)
    {
        printf("error: usage mic_level [<ms>]\r\n");
        return;
    }

    if (command->argc == 1)
    {
        if (!parse_u32(command->argv[0], &window_ms))
        {
            printf("error: usage mic_level [<ms>]\r\n");
            return;
        }
    }

    uint64_t count64 = ((uint64_t)window_ms * (uint64_t)PLATFORM_SAMPLE_RATE_HZ) / 1000ull;
    if (count64 == 0u || count64 > (uint64_t)PLATFORM_BLOCK_MAX)
    {
        printf("error: ms must be 1..185\r\n");
        return;
    }

    if (!start_block((uint32_t)count64, print_mic_level))
    {
        printf("error: adc busy\r\n");
    }
}

void api_send_frame(const stream_header_t *header, const uint8_t *data, uint32_t bytes)
{
    // байты заголовка и отсчётов как они лежат в памяти
    platform_write(header, (uint32_t)sizeof(*header));
    platform_write(data, bytes);
}

static void cmd_stream_start(const command_t *command)
{
    uint32_t bits = 16u;

    if (command->argc > 1)
    {
        printf("error: usage stream_start [8|16]\r\n");
        return;
    }

    if (command->argc == 1)
    {
        if (!parse_u32(command->argv[0], &bits) || (bits != 8u && bits != 16u))
        {
            printf("error: usage stream_start [8|16]\r\n");
            return;
        }
    }

    if (telemetry_task_get_state() == TELEMETRY_STATE_RUN || !stream_task_start((uint8_t)bits))
    {
        printf("error: adc busy\r\n");
        return;
    }

    printf("stream: started, %u Hz, %lu bit\r\n",
           (unsigned)PLATFORM_SAMPLE_RATE_HZ, (unsigned long)bits);

    // индикатор записи: мигание с периодом STREAM_BLINK_PERIOD_MS,
    // задачу светодиода — в состояние «выключен»
    (void)platform_blink_start(STREAM_BLINK_PERIOD_MS);
    led_task_set_state(LED_STATE_OFF);
}

static void cmd_stream_stop(const command_t *command)
{
    (void)command;

    if (stream_task_get_state() == STREAM_STATE_RUN)
    {
        stream_task_stop();
        (void)platform_blink_stop();
    }

    printf("stream: stopped, %lu frames, %lu samples, %lu overruns\r\n",
           (unsigned long)stream_task_get_frames(),
           (unsigned long)stream_task_get_samples(),
           (unsigned long)stream_task_get_overruns());
}

static void cmd_wdt_enable(const command_t *command)
{
    if (command->argc != 1)
    {
        printf("error: usage wdt_enable <timeout_ms>\r\n");
        return;
    }

    uint32_t timeout_ms = 0u;
    if (!parse_u32(command->argv[0], &timeout_ms))
    {
        printf("error: usage wdt_enable <timeout_ms>\r\n");
        return;
    }

    if (!platform_watchdog_enable(timeout_ms))
    {
        printf("error: timeout_ms must be 1..8388\r\n");
        return;
    }

    printf("wdt: enabled, timeout %lu ms\r\n", (unsigned long)timeout_ms);
}

static void cmd_hang(const command_t *command)
{
    (void)command;
    printf("hang: superloop stopped\r\n");

    // суперцикл больше не идёт, прерывания приходят как прежде
    while (1)
    {
    }
}

static void cmd_reset_reason(const command_t *command)
{
    (void)command;
    uint32_t restarts = 0u;
    bool watchdog = platform_watchdog_caused_reset(&restarts);
    if (watchdog)
    {
        printf("reset: watchdog, restarts %lu\r\n", (unsigned long)restarts);
    }
    else
    {
        printf("reset: other, restarts %lu\r\n", (unsigned long)restarts);
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
    {"get_voltage", "voltage on GP26, ADC input 0", cmd_get_voltage},
    {"get_temp", "chip temperature, degrees Celsius", cmd_get_temp},
    {"tm_start", "start telemetry: tm_start [<period_ms>]", cmd_tm_start},
    {"tm_stop", "stop telemetry", cmd_tm_stop},
    {"charge_on", "charge RC circuit through GP22", cmd_charge_on},
    {"charge_off", "discharge RC circuit through GP22", cmd_charge_off},
    {"timer_blink_start", "blink led by timer: timer_blink_start <period_ms>", cmd_timer_blink_start},
    {"timer_blink_stop", "stop timer blink, number of toggles", cmd_timer_blink_stop},
    {"cap_start", "capture by timer: cap_start <N> <period_us>", cmd_cap_start},
    {"cap_dump", "dump timer capture", cmd_cap_dump},
    {"dma_cap", "capture block by DMA: dma_cap <N>", cmd_dma_cap},
    {"generate_tone", "tone on GP20: generate_tone <Hz>", cmd_generate_tone},
    {"mic_bias", "microphone DC bias", cmd_mic_bias},
    {"mic_level", "microphone level: mic_level [<ms>]", cmd_mic_level},
    {"stream_start", "start stream: stream_start [8|16]", cmd_stream_start},
    {"stream_stop", "stop stream", cmd_stream_stop},
    {"wdt_enable", "enable watchdog: wdt_enable <timeout_ms>", cmd_wdt_enable},
    {"hang", "stop superloop forever", cmd_hang},
    {"reset_reason", "reason of last reset", cmd_reset_reason},
};

const uint32_t api_command_count = (uint32_t)(sizeof(api_commands) / sizeof(api_commands[0]));
