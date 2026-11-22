#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// запускает стандартный ввод-вывод и настраивает выводы
void platform_init(void);

void platform_led_set(bool on);

// нажата ли кнопка сейчас, без подавления дребезга
bool platform_button_read(void);

// режим вывода GP22, который ведёт линию RC-цепи
typedef enum {
    PLATFORM_LINE_PUSH_PULL,
    PLATFORM_LINE_OPEN_DRAIN,
} platform_line_mode_t;

// единица или ноль на линии; в режиме открытого стока единица —
// отпущенная линия, которую поднимает внутренняя подтяжка
void platform_line_set(platform_line_mode_t mode, bool high);

// напряжение на входе АЦП, вывод GP26, в вольтах
float platform_voltage_read(void);

// модули UART: UART0 — GP0 (TX) и GP1 (RX), UART1 — GP8 (TX) и GP9 (RX)
typedef enum {
    PLATFORM_UART0,
    PLATFORM_UART1,
} platform_uart_t;

typedef enum {
    PLATFORM_PARITY_NONE,
    PLATFORM_PARITY_EVEN,
    PLATFORM_PARITY_ODD,
} platform_parity_t;

// признаки ошибок приёма, биты регистра UARTRSR
#define PLATFORM_UART_FE 0x1
#define PLATFORM_UART_PE 0x2
#define PLATFORM_UART_BE 0x4
#define PLATFORM_UART_OE 0x8

// true — петля LBE внутри UART0, выводы GP0 и GP1 модулю не отданы;
// false — петли нет, выводы отданы UART0
void platform_uart_loop_set(bool internal);

// очередь приёма пуста, признаки ошибок сброшены
void platform_uart_clear(platform_uart_t uart);

// кладёт байт в очередь передачи; false — очередь полна
bool platform_uart_put(platform_uart_t uart, uint8_t byte);

// передатчик ещё работает: байты в очереди или в сдвиговом регистре
bool platform_uart_busy(platform_uart_t uart);

// всё принятое с прошлой очистки, не больше size байтов
size_t platform_uart_read(platform_uart_t uart, uint8_t *data, size_t size);

// признаки ошибок приёма с прошлой очистки
uint32_t platform_uart_errors(platform_uart_t uart);

// возвращает скорость, которая получилась после округления делителя
uint32_t platform_uart_baud_set(platform_uart_t uart, uint32_t baud);

void platform_uart_parity_set(platform_uart_t uart, platform_parity_t parity);

// прерывание приёма UART0: принятые байты забирает обработчик
void platform_uart_rx_irq_set(bool on);

// частота отсчётов АЦП в блоке: предел модуля, 96 тактов clk_adc на отсчёт
#define PLATFORM_SAMPLE_RATE_HZ 500000

// 8,2 мс линии: символ на 9600 бит/с длится 1 мс
#define PLATFORM_BLOCK_MAX 4096

// false — АЦП занят или count больше PLATFORM_BLOCK_MAX
bool platform_block_start(uint32_t count);

// блок готов, данные можно забирать
bool platform_block_ready(void);

// отсчёты последнего блока
const uint16_t *platform_block_samples(void);

// сколько отсчётов в последнем блоке
uint32_t platform_block_count(void);

// сколько микросекунд занял захват последнего блока
uint32_t platform_block_time_us(void);
