#include "platform/platform.h"

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/uart.h"

#define PLATFORM_LED_PIN PICO_DEFAULT_LED_PIN
#define PLATFORM_BUTTON_PIN 15

// вывод, который через резистор 1 кОм ведёт линию RC-цепи
#define LINE_PIN 22

// вход АЦП: вывод GP26, канал 0
#define ADC_PIN 26
#define ADC_CHANNEL 0

// опорное напряжение АЦП на плате Pico и число уровней 12-разрядного кода
#define ADC_VREF_V 3.3f
#define ADC_LEVELS 4096.0f

// выводы модуля UART0
#define UART0_TX_PIN 0
#define UART0_RX_PIN 1

// выводы модуля UART1
#define UART1_TX_PIN 8
#define UART1_RX_PIN 9

#define UART_BAUD 115200

// принятое обработчиком прерывания: блок длиннее очереди приёма
#define RX_BUFFER_SIZE 128

static volatile uint8_t rx_buffer[RX_BUFFER_SIZE];
static volatile uint32_t rx_count;
static volatile uint32_t rx_errors;

typedef enum {
    ADC_FREE = 0,
    ADC_BLOCK,
} adc_user_t;

static volatile adc_user_t adc_user = ADC_FREE;
static volatile bool block_ready = false;
static volatile uint32_t block_count = 0;
static volatile uint32_t block_time_us = 0;
static uint16_t block_samples[PLATFORM_BLOCK_MAX];
static uint64_t block_start_us = 0;
static int block_dma_channel = -1;

static uart_inst_t *uart_of(platform_uart_t uart)
{
    return uart == PLATFORM_UART0 ? uart0 : uart1;
}

// обработчик UART0_IRQ: забирает из очереди приёма всё, что там есть
static void on_uart0_rx(void)
{
    while (uart_is_readable(uart0))
    {
        uint32_t dr = uart_get_hw(uart0)->dr;
        uint8_t byte = (uint8_t)(dr & 0xffu);

        if (rx_count < RX_BUFFER_SIZE)
        {
            rx_buffer[rx_count++] = byte;
        }

        // биты 11:8 регистра данных несут признаки ошибок этого байта
        rx_errors |= (dr >> 8) & 0xFu;
    }
}

static void adc_free_running(void)
{
    adc_select_input(ADC_CHANNEL);
    adc_run(false);
    adc_fifo_drain();

    // предел АЦП: преобразование длится 96 тактов clk_adc, поэтому делитель 0
    adc_set_clkdiv(0);
    adc_fifo_setup(
        true,  // fifo enable
        true,  // dreq enable
        1,     // dreq when at least 1 sample
        false, // no ERR bit
        false  // keep 12-bit samples, no SHIFT to 8 bit
    );
    adc_run(true);
}

static void adc_one_shot(void)
{
    adc_run(false);
    adc_fifo_drain();
    adc_fifo_setup(false, false, 0, false, false);
    adc_select_input(ADC_CHANNEL);
}

static void on_block_done(void)
{
    // DMA заполнил весь блок: остановить АЦП и вернуть одиночные измерения
    dma_channel_acknowledge_irq1(block_dma_channel);
    adc_run(false);
    adc_fifo_drain();
    block_time_us = (uint32_t)(time_us_64() - block_start_us);
    block_ready = true;
    adc_user = ADC_FREE;
    adc_one_shot();
}

void platform_init(void)
{
    stdio_init_all();

    gpio_init(PLATFORM_LED_PIN);
    gpio_set_dir(PLATFORM_LED_PIN, GPIO_OUT);
    gpio_put(PLATFORM_LED_PIN, 0);

    gpio_init(PLATFORM_BUTTON_PIN);
    gpio_set_dir(PLATFORM_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(PLATFORM_BUTTON_PIN);

    adc_init();
    adc_gpio_init(ADC_PIN);
    adc_select_input(ADC_CHANNEL);

    gpio_init(LINE_PIN);
    platform_line_set(PLATFORM_LINE_PUSH_PULL, false);

    // делитель, формат 8N1, очереди, включение модуля
    uart_init(uart0, 115200);
    platform_uart_loop_set(false);

    uart_init(uart1, 115200);
    gpio_set_function(UART1_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART1_RX_PIN, GPIO_FUNC_UART);

    irq_set_exclusive_handler(UART0_IRQ, on_uart0_rx);
    irq_set_enabled(UART0_IRQ, true);

    block_dma_channel = dma_claim_unused_channel(true);
    dma_channel_set_irq1_enabled(block_dma_channel, true);
    irq_set_exclusive_handler(DMA_IRQ_1, on_block_done);
    irq_set_enabled(DMA_IRQ_1, true);
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

void platform_line_set(platform_line_mode_t mode, bool high)
{
    if (mode == PLATFORM_LINE_PUSH_PULL)
    {
        // драйвер включён и сам задаёт уровень
        gpio_disable_pulls(LINE_PIN);
        gpio_put(LINE_PIN, high);
        gpio_set_dir(LINE_PIN, GPIO_OUT);
        return;
    }

    // открытый сток: в регистре выхода ноль, переключается только режим
    gpio_pull_up(LINE_PIN);
    gpio_put(LINE_PIN, 0);
    gpio_set_dir(LINE_PIN, high ? GPIO_IN : GPIO_OUT);
}

float platform_voltage_read(void)
{
    adc_select_input(ADC_CHANNEL);
    return (float)adc_read() * ADC_VREF_V / ADC_LEVELS;
}

void platform_uart_loop_set(bool internal)
{
    uint function = internal ? GPIO_FUNC_NULL : GPIO_FUNC_UART;
    gpio_set_function(UART0_TX_PIN, function);
    gpio_set_function(UART0_RX_PIN, function);

    if (internal)
    {
        hw_set_bits(&uart_get_hw(uart0)->cr, UART_UARTCR_LBE_BITS);
    }
    else
    {
        hw_clear_bits(&uart_get_hw(uart0)->cr, UART_UARTCR_LBE_BITS);
    }
}

void platform_uart_clear(platform_uart_t uart)
{
    uart_inst_t *u = uart_of(uart);

    while (uart_is_readable(u))
    {
        (void)uart_get_hw(u)->dr;
    }

    // запись в UARTECR сбрасывает признаки ошибок
    uart_get_hw(u)->rsr = 0;

    if (uart == PLATFORM_UART0)
    {
        rx_count = 0;
        rx_errors = 0;
    }
}

bool platform_uart_put(platform_uart_t uart, uint8_t byte)
{
    uart_inst_t *u = uart_of(uart);

    if (!uart_is_writable(u))
    {
        return false;
    }

    // место в очереди есть, поэтому uart_putc_raw() не ждёт
    uart_putc_raw(u, byte);
    return true;
}

bool platform_uart_busy(platform_uart_t uart)
{
    return (uart_get_hw(uart_of(uart))->fr & UART_UARTFR_BUSY_BITS) != 0;
}

size_t platform_uart_read(platform_uart_t uart, uint8_t *data, size_t size)
{
    size_t n = 0;

    if (uart == PLATFORM_UART0)
    {
        uint32_t count = rx_count;
        if (count > RX_BUFFER_SIZE)
        {
            count = RX_BUFFER_SIZE;
        }
        for (uint32_t i = 0; i < count && n < size; i++)
        {
            data[n++] = rx_buffer[i];
        }
    }

    uart_inst_t *u = uart_of(uart);
    while (uart_is_readable(u) && n < size)
    {
        data[n++] = (uint8_t)(uart_get_hw(u)->dr & 0xffu);
    }

    return n;
}

uint32_t platform_uart_errors(platform_uart_t uart)
{
    uint32_t errors = uart_get_hw(uart_of(uart))->rsr & 0xFu;
    if (uart == PLATFORM_UART0)
    {
        errors |= rx_errors;
    }
    return errors;
}

uint32_t platform_uart_baud_set(platform_uart_t uart, uint32_t baud)
{
    return uart_set_baudrate(uart_of(uart), baud);
}

void platform_uart_parity_set(platform_uart_t uart, platform_parity_t parity)
{
    static const uart_parity_t parities[] = {
        UART_PARITY_NONE,
        UART_PARITY_EVEN,
        UART_PARITY_ODD,
    };

    uart_set_format(uart_of(uart), 8, 1, parities[parity]);
}

void platform_uart_rx_irq_set(bool on)
{
    // прерывание по четырём байтам в очереди и по паузе на линии
    uart_set_irqs_enabled(uart0, on, false);
}

bool platform_block_start(uint32_t count)
{
    if (adc_user != ADC_FREE || count == 0u || count > PLATFORM_BLOCK_MAX)
    {
        return false;
    }

    adc_user = ADC_BLOCK;
    block_ready = false;
    block_count = count;
    block_time_us = 0;

    adc_free_running();
    block_start_us = time_us_64();

    dma_channel_config cfg = dma_channel_get_default_config(block_dma_channel);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);
    channel_config_set_dreq(&cfg, DREQ_ADC);

    dma_channel_configure(
        block_dma_channel,
        &cfg,
        block_samples,
        &adc_hw->fifo,
        count,
        true
    );

    return true;
}

bool platform_block_ready(void)
{
    return block_ready;
}

const uint16_t *platform_block_samples(void)
{
    return (const uint16_t *)block_samples;
}

uint32_t platform_block_count(void)
{
    return block_count;
}

uint32_t platform_block_time_us(void)
{
    return block_time_us;
}
