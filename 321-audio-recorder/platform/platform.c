#include "platform/platform.h"

#include <stddef.h>

#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "hardware/watchdog.h"

#define PLATFORM_LED_PIN PICO_DEFAULT_LED_PIN
#define PLATFORM_BUTTON_PIN 15u

// вход АЦП: вывод GP26, канал 0
#define ADC_PIN 26u
#define ADC_CHANNEL 0u

// встроенный датчик температуры - канал 4
#define ADC_TEMP_CHANNEL 4u

// вывод, через резистор заряжающий конденсатор RC-цепи
#define CHARGE_PIN 22u

// тон на GP20: меандр частоты freq_hz от модуля ШИМ
#define TONE_PIN 20u

// счёт ШИМ тона: делитель приводит clk_sys к 1 МГц, счётчик считает микросекунды
#define TONE_COUNT_HZ 1000000u

// опорное напряжение АЦП на плате Pico и число уровней 12-разрядного кода
#define ADC_VREF_V 3.3f
#define ADC_LEVELS 4096.0f

// частота clk_adc, от неё делитель отсчитывает шаг
#define ADC_CLOCK_HZ 48000000.0f

// наибольший таймаут сторожевого таймера RP2040 (RP2040-E1)
#define WATCHDOG_MAX_MS 8388u

// кто сейчас работает с АЦП; меняют и функции, и обработчики прерываний
typedef enum
{
    ADC_FREE = 0,
    ADC_CAPTURE,
    ADC_BLOCK,
    ADC_STREAM,
} adc_user_t;

static volatile adc_user_t adc_user = ADC_FREE;

// --- индикатор записи по будильнику ---
static repeating_timer_t blink_timer;
static bool blink_running = false;
static volatile bool blink_on = false;
static volatile uint32_t blink_toggles = 0u;

// --- захват по будильнику ---
static repeating_timer_t capture_timer;
static uint16_t capture_samples[PLATFORM_CAPTURE_MAX];
static uint32_t capture_times[PLATFORM_CAPTURE_MAX];
static uint32_t capture_count = 0u;
static uint32_t capture_period_us = 0u;
static volatile uint32_t capture_index = 0u;
static volatile uint32_t capture_handler_us = 0u;

// --- захват блока через DMA ---
static int block_channel = -1;
static uint16_t block_samples[PLATFORM_BLOCK_MAX];
static uint32_t block_count = 0u;
static uint32_t block_start_us = 0u;
static volatile uint32_t block_end_us = 0u;
static volatile bool block_ready = false;

// --- поток с двойной буферизацией ---
static int stream_channels[2] = {-1, -1};
static uint8_t stream_buffers[2][PLATFORM_STREAM_BYTES] __attribute__((aligned(4)));
static volatile bool stream_ready[2] = {false, false};
static volatile uint32_t stream_overruns = 0u;
static uint32_t stream_samples = 0u;
static uint32_t stream_next = 0u;
static uint8_t stream_bits = 16u;

// --- сторожевой таймер ---
static bool watchdog_reset = false;
static uint32_t watchdog_restarts = 0u;

// --- обработчик будильника индикатора ---
static bool on_blink(repeating_timer_t *timer)
{
    (void)timer;
    blink_on = !blink_on;
    gpio_put(PLATFORM_LED_PIN, blink_on);
    blink_toggles++;

    // true — вызвать снова через тот же период
    return true;
}

// --- обработчик будильника захвата: время и отсчёт, затем время своей работы ---
static bool on_capture(repeating_timer_t *timer)
{
    (void)timer;
    uint32_t start_us = time_us_32();
    uint32_t index = capture_index;
    capture_times[index] = start_us;
    capture_samples[index] = adc_read();
    index++;
    capture_index = index;
    capture_handler_us += time_us_32() - start_us;

    if (index < capture_count)
    {
        return true;
    }

    // захват окончен: АЦП свободен, будильник снимается
    adc_user = ADC_FREE;
    return false;
}

// АЦП в непрерывном режиме с шагом 44,1 кГц, буфер АЦП с запросом DMA
// на каждый отсчёт; bits8 — модуль сам отбрасывает 4 младших разряда
static void adc_free_running(bool bits8)
{
    adc_select_input(ADC_CHANNEL);
    adc_fifo_setup(true, true, 1, false, bits8);
    adc_set_clkdiv(ADC_CLOCK_HZ / (float)PLATFORM_SAMPLE_RATE_HZ - 1.0f);
}

// АЦП обратно в режим отсчётов по команде
static void adc_one_shot(void)
{
    adc_run(false);
    adc_fifo_drain();
    adc_fifo_setup(false, false, 0, false, false);
    adc_set_clkdiv(0.0f);
}

// обработчик прерывания DMA_IRQ_1: блок захвачен
static void on_block_done(void)
{
    dma_channel_acknowledge_irq1(block_channel);
    block_end_us = time_us_32();
    adc_one_shot();
    block_ready = true;
    adc_user = ADC_FREE;
}

// обработчик прерывания DMA_IRQ_0: канал заполнил свой буфер
static void on_stream_buffer(void)
{
    for (int i = 0; i < 2; i++)
    {
        if (dma_channel_get_irq0_status(stream_channels[i]))
        {
            dma_channel_acknowledge_irq0(stream_channels[i]);

            // адрес записи дошёл до конца буфера, вернуть его к началу
            dma_channel_set_write_addr(stream_channels[i], stream_buffers[i], false);

            // в следующий буфер уже идёт запись, а его ещё не отправили
            int next = (i + 1) % 2;
            if (stream_ready[next])
            {
                stream_overruns++;
            }
            stream_ready[i] = true;
        }
    }
}

void platform_init(void)
{
    // причина прошлого сброса; счётчик сбросов подряд хранит scratch-регистр 0,
    // его не стирает сброс по сторожевому таймеру
    watchdog_reset = watchdog_enable_caused_reboot();
    if (watchdog_reset)
    {
        watchdog_hw->scratch[0]++;
    }
    else
    {
        watchdog_hw->scratch[0] = 0u;
    }
    watchdog_restarts = watchdog_hw->scratch[0];

    stdio_init_all();

    gpio_init(PLATFORM_LED_PIN);
    gpio_set_dir(PLATFORM_LED_PIN, GPIO_OUT);
    gpio_put(PLATFORM_LED_PIN, 0);

    gpio_init(PLATFORM_BUTTON_PIN);
    gpio_set_dir(PLATFORM_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(PLATFORM_BUTTON_PIN);

    gpio_init(CHARGE_PIN);
    gpio_set_dir(CHARGE_PIN, GPIO_OUT);
    gpio_put(CHARGE_PIN, 0);

    adc_init();
    adc_gpio_init(ADC_PIN);
    adc_set_temp_sensor_enabled(true);

    // тон на GP20: ШИМ, счёт раз в микросекунду, порог 0 — на выводе ноль
    gpio_set_function(TONE_PIN, GPIO_FUNC_PWM);
    uint tone_slice = pwm_gpio_to_slice_num(TONE_PIN);
    pwm_config tone_cfg = pwm_get_default_config();
    float clk_sys_hz = (float)clock_get_hz(clk_sys);
    pwm_config_set_clkdiv(&tone_cfg, clk_sys_hz / (float)TONE_COUNT_HZ);
    pwm_init(tone_slice, &tone_cfg, true);
    pwm_set_gpio_level(TONE_PIN, 0u);

    // канал блока сообщает о конце через DMA_IRQ_1
    block_channel = dma_claim_unused_channel(true);
    dma_channel_set_irq1_enabled(block_channel, true);
    irq_set_exclusive_handler(DMA_IRQ_1, on_block_done);
    irq_set_enabled(DMA_IRQ_1, true);

    // два канала потока сообщают о заполненном буфере через DMA_IRQ_0
    stream_channels[0] = dma_claim_unused_channel(true);
    stream_channels[1] = dma_claim_unused_channel(true);
    irq_set_exclusive_handler(DMA_IRQ_0, on_stream_buffer);
    irq_set_enabled(DMA_IRQ_0, true);
}

void platform_led_set(bool on)
{
    gpio_put(PLATFORM_LED_PIN, on ? 1 : 0);
}

bool platform_button_read(void)
{
    // нажатая кнопка замыкает вывод на землю и даёт ноль
    return !gpio_get(PLATFORM_BUTTON_PIN);
}

float platform_adc_volts(float code)
{
    return code * ADC_VREF_V / ADC_LEVELS;
}

float platform_voltage_read(void)
{
    adc_select_input(ADC_CHANNEL);
    return platform_adc_volts((float)adc_read());
}

float platform_temp_read(void)
{
    adc_select_input(ADC_TEMP_CHANNEL);
    float u = platform_adc_volts((float)adc_read());
    // перевод напряжения в градусы по формуле из RP2040 Datasheet
    return 27.0f - (u - 0.706f) / 0.001721f;
}

void platform_charge_set(bool on)
{
    gpio_put(CHARGE_PIN, on ? 1 : 0);
}

bool platform_blink_start(uint32_t period_ms)
{
    if (period_ms < 10u || period_ms > 60000u)
    {
        return false;
    }

    (void)platform_blink_stop();
    blink_toggles = 0u;

    // минус перед периодом — отсчитывать от начала прошлого вызова
    blink_running = add_repeating_timer_ms(-(int32_t)period_ms, on_blink, NULL, &blink_timer);
    return blink_running;
}

uint32_t platform_blink_stop(void)
{
    if (blink_running)
    {
        cancel_repeating_timer(&blink_timer);
        blink_running = false;
    }

    blink_on = false;
    gpio_put(PLATFORM_LED_PIN, 0);
    return blink_toggles;
}

bool platform_adc_busy(void)
{
    return adc_user != ADC_FREE;
}

bool platform_capture_start(uint32_t count, uint32_t period_us)
{
    if (adc_user != ADC_FREE || count == 0u || count > PLATFORM_CAPTURE_MAX || period_us == 0u)
    {
        return false;
    }

    adc_select_input(ADC_CHANNEL);
    capture_count = count;
    capture_period_us = period_us;
    capture_index = 0u;
    capture_handler_us = 0u;
    adc_user = ADC_CAPTURE;

    if (!add_repeating_timer_us(-(int64_t)period_us, on_capture, NULL, &capture_timer))
    {
        adc_user = ADC_FREE;
        return false;
    }
    return true;
}

bool platform_capture_done(void)
{
    // захват запускали, и набраны все отсчёты
    return capture_count > 0u && capture_index >= capture_count && adc_user != ADC_CAPTURE;
}

void platform_capture_get(platform_capture_t *capture)
{
    if (capture == NULL)
    {
        return;
    }

    capture->samples = capture_samples;
    capture->count = capture_count;
    capture->period_us = capture_period_us;
    capture->min_interval_us = 0u;
    capture->max_interval_us = 0u;
    capture->handler_percent = 0.0f;

    if (capture_count < 2u)
    {
        return;
    }

    uint32_t min_interval = 0xFFFFFFFFu;
    uint32_t max_interval = 0u;
    for (uint32_t i = 1u; i < capture_count; i++)
    {
        uint32_t interval = capture_times[i] - capture_times[i - 1u];
        if (interval < min_interval)
        {
            min_interval = interval;
        }
        if (interval > max_interval)
        {
            max_interval = interval;
        }
    }

    capture->min_interval_us = min_interval;
    capture->max_interval_us = max_interval;

    uint32_t total_us = capture_times[capture_count - 1u] - capture_times[0];
    if (total_us > 0u)
    {
        capture->handler_percent = (float)capture_handler_us * 100.0f / (float)total_us;
    }
}

bool platform_block_start(uint32_t count)
{
    if (adc_user != ADC_FREE || count == 0u || count > PLATFORM_BLOCK_MAX)
    {
        return false;
    }

    block_count = count;
    adc_free_running(false);

    // 16-битные передачи из одного адреса в растущий, по запросу АЦП
    dma_channel_config cfg = dma_channel_get_default_config(block_channel);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);
    channel_config_set_dreq(&cfg, DREQ_ADC);

    block_ready = false;
    adc_user = ADC_BLOCK;

    // куда, откуда, сколько; true — запустить сразу
    dma_channel_configure(block_channel, &cfg, block_samples, &adc_hw->fifo, count, true);
    block_start_us = time_us_32();
    adc_run(true);
    return true;
}

bool platform_block_done(void)
{
    return block_ready;
}

const uint16_t *platform_block_samples(void)
{
    return block_samples;
}

uint32_t platform_block_time_us(void)
{
    return block_end_us - block_start_us;
}

bool platform_stream_start(uint8_t bits)
{
    if (adc_user != ADC_FREE || (bits != 8u && bits != 16u))
    {
        return false;
    }

    stream_bits = bits;
    stream_samples = (bits == 8u) ? PLATFORM_STREAM_BYTES : (PLATFORM_STREAM_BYTES / 2u);
    adc_free_running(bits == 8u);

    // канал i пишет в буфер i и по окончании запускает другой канал
    for (int i = 0; i < 2; i++)
    {
        // номер следующего канала в цепочке
        int next_channel = stream_channels[(i + 1) % 2];

        dma_channel_config cfg = dma_channel_get_default_config(stream_channels[i]);
        channel_config_set_transfer_data_size(&cfg, bits == 8u ? DMA_SIZE_8 : DMA_SIZE_16);
        channel_config_set_read_increment(&cfg, false);
        channel_config_set_write_increment(&cfg, true);
        channel_config_set_dreq(&cfg, DREQ_ADC);
        channel_config_set_chain_to(&cfg, next_channel);
        dma_channel_configure(stream_channels[i], &cfg, stream_buffers[i], &adc_hw->fifo,
                              stream_samples, false);
        dma_channel_set_irq0_enabled(stream_channels[i], true);
        stream_ready[i] = false;
    }

    stream_overruns = 0u;
    stream_next = 0u;
    adc_user = ADC_STREAM;
    dma_channel_start(stream_channels[0]);
    adc_run(true);
    return true;
}

void platform_stream_stop(void)
{
    if (adc_user != ADC_STREAM)
    {
        return;
    }

    // сначала остановить отсчёты, затем каналы и их прерывания
    adc_run(false);

    // RP2040-E13: передача, начатая до прерывания канала, завершается как обычная
    // и запускает канал из цепочки. Поэтому цепочка сначала разрывается: каждый
    // канал указывает сам на себя, иначе один из них останется взведённым
    for (int i = 0; i < 2; i++)
    {
        dma_channel_config cfg = dma_get_channel_config(stream_channels[i]);
        channel_config_set_chain_to(&cfg, stream_channels[i]);
        dma_channel_set_config(stream_channels[i], &cfg, false);
    }

    for (int i = 0; i < 2; i++)
    {
        dma_channel_set_irq0_enabled(stream_channels[i], false);
        dma_channel_abort(stream_channels[i]);
        dma_channel_acknowledge_irq0(stream_channels[i]);
    }

    adc_one_shot();
    adc_user = ADC_FREE;
}

const uint8_t *platform_stream_next(uint32_t *samples)
{
    uint32_t index = stream_next;
    if (!stream_ready[index])
    {
        return NULL;
    }

    if (samples != NULL)
    {
        *samples = stream_samples;
    }
    return stream_buffers[index];
}

void platform_stream_release(void)
{
    uint32_t index = stream_next;
    stream_ready[index] = false;
    stream_next = (index + 1u) % 2u;
}

uint32_t platform_stream_overruns(void)
{
    return stream_overruns;
}

void platform_write(const void *data, uint32_t bytes)
{
    // false, false — без перевода строки в конце и без замены \n на \r\n
    stdio_put_string((const char *)data, (int)bytes, false, false);
}

bool platform_tone_set(uint32_t freq_hz)
{
    if (freq_hz == 0u)
    {
        pwm_set_gpio_level(TONE_PIN, 0u);
        return true;
    }

    if (freq_hz < 20u || freq_hz > 20000u)
    {
        return false;
    }

    uint32_t period_us = TONE_COUNT_HZ / freq_hz;
    if (period_us == 0u)
    {
        return false;
    }

    uint slice = pwm_gpio_to_slice_num(TONE_PIN);
    pwm_set_wrap(slice, (uint16_t)(period_us - 1u));
    pwm_set_gpio_level(TONE_PIN, (uint16_t)(period_us / 2u));
    return true;
}

bool platform_watchdog_enable(uint32_t timeout_ms)
{
    if (timeout_ms == 0u || timeout_ms > WATCHDOG_MAX_MS)
    {
        return false;
    }

    // true — не считать, пока ядро остановлено отладчиком
    watchdog_enable(timeout_ms, true);
    return true;
}

void platform_watchdog_feed(void)
{
    watchdog_update();
}

bool platform_watchdog_caused_reset(uint32_t *restarts)
{
    if (restarts != NULL)
    {
        *restarts = watchdog_restarts;
    }
    return watchdog_reset;
}
