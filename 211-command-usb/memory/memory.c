#include "memory.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/regs/addressmap.h"
#include "led.h"
#include "device.h"
#include "command.h"

extern char __flash_binary_start;
extern char __flash_binary_end;
extern char __boot2_start__;
extern char __boot2_end__;
extern char __etext;
extern char __data_start__;
extern char __data_end__;
extern char __bss_start__;
extern char __bss_end__;
extern char __HeapLimit;
extern char __StackBottom;
extern char __StackTop;

int main(void);

uint32_t data_variable = 100;
uint32_t bss_variable;


// Размеры ОЗУ и ПЗУ — из RP2040 Datasheet, раздел 2.2 Address Map
#define SRAM_SIZE (264 * 1024)   // 264 КБ
#define ROM_SIZE  (16 * 1024)    // 16 КБ

static void row(const char* name, uintptr_t start, uintptr_t end)
{
    printf("%-10s 0x%08x 0x%08x %8u\n",
        name, (unsigned)start, (unsigned)end, (unsigned)(end - start));
}

void mem_info(void)
{
    // --- Расчет размеров из символов компоновщика ---
    unsigned boot2_sz = (unsigned)((uintptr_t)&__boot2_end__ - (uintptr_t)&__boot2_start__);
    unsigned text_sz = (unsigned)((uintptr_t)&__etext - (uintptr_t)&__boot2_end__);
    unsigned data_sz = (unsigned)((uintptr_t)&__data_end__ - (uintptr_t)&__data_start__);
    unsigned bss_sz = (unsigned)((uintptr_t)&__bss_end__ - (uintptr_t)&__bss_start__);
    unsigned heap_sz = (unsigned)((uintptr_t)&__HeapLimit - (uintptr_t)&__bss_end__);
    unsigned stack_sz = (unsigned)((uintptr_t)&__StackTop - (uintptr_t)&__StackBottom);

    unsigned flash_img_sz = boot2_sz + text_sz + data_sz;
    unsigned flash_total_sz = (unsigned)PICO_FLASH_SIZE_BYTES;
    unsigned flash_free_sz = flash_total_sz - flash_img_sz;
    unsigned ram_used = data_sz + bss_sz;

    // --- Вывод таблицы ---
    printf("%-10s %-10s %-10s %8s\n", "area", "start", "end", "size");

    // Железо
    row("flash", XIP_BASE, XIP_BASE + PICO_FLASH_SIZE_BYTES);
    row("sram", SRAM_BASE, SRAM_BASE + SRAM_SIZE);
    row("rom", ROM_BASE, ROM_BASE + ROM_SIZE);

    // Секции Flash
    row("image", (uintptr_t)&__flash_binary_start, (uintptr_t)&__flash_binary_end);
    row("free", (uintptr_t)&__flash_binary_end, XIP_BASE + PICO_FLASH_SIZE_BYTES);

    row("boot2", (uintptr_t)&__boot2_start__, (uintptr_t)&__boot2_end__);
    row("text", (uintptr_t)&__boot2_end__, (uintptr_t)&__etext);
    row("data flash", (uintptr_t)&__etext, (uintptr_t)&__etext + data_sz);

    // Секции RAM
    row("data ram", (uintptr_t)&__data_start__, (uintptr_t)&__data_end__);
    row("bss", (uintptr_t)&__bss_start__, (uintptr_t)&__bss_end__);
    row("heap", (uintptr_t)&__bss_end__, (uintptr_t)&__HeapLimit);
    row("stack", (uintptr_t)&__StackBottom, (uintptr_t)&__StackTop);

    // --- Вывод итогов  ---
    printf("\ntotal\n");
    printf("  flash image    %5u = boot2 %u + text %u + data %u\n",
        flash_img_sz, boot2_sz, text_sz, data_sz);

    printf("  flash free   %7u of %u\n",
        flash_free_sz, flash_total_sz);

    printf("  ram used        %5u = data %u + bss %u\n",
        ram_used, data_sz, bss_sz);

    printf("  ram free      %6u for heap and %u for stack\n",
        heap_sz, stack_sz);
}


void fw_info(void)
{
    data_variable = data_variable + 1;
    bss_variable = bss_variable + 1;

    uint16_t *main_code = (uint16_t *)((uintptr_t)main & ~1u);
    uint16_t *fw_info_code = (uint16_t *)((uintptr_t)fw_info & ~1u);

    printf("object          address     value\n");
    printf("main            0x%08x  0x%04x\n",
           (unsigned)(uintptr_t)main, *main_code);
    printf("fw_info         0x%08x  0x%04x\n",
           (unsigned)(uintptr_t)fw_info, *fw_info_code);

    printf("commands        0x%08x\n", (unsigned)(uintptr_t)commands);
    for (uint i = 0; i < command_count; i++)
    {
        printf("- %-13s 0x%08x\n",
               commands[i].name,
               (unsigned)(uintptr_t)commands[i].handler);
    }

    printf("DEVICE_PROJECT  0x%08x  %s\n",
           (unsigned)(uintptr_t)DEVICE_PROJECT, DEVICE_PROJECT);
    printf("DEVICE_BOARD    0x%08x  %s\n",
           (unsigned)(uintptr_t)DEVICE_BOARD, DEVICE_BOARD);

    printf("data_variable   0x%08x  %u\n",
           (unsigned)(uintptr_t)&data_variable, data_variable);
    printf("bss_variable    0x%08x  %u\n",
           (unsigned)(uintptr_t)&bss_variable, bss_variable);

    uint32_t stack_variable = 1946;
    uint32_t *heap_variable = malloc(sizeof(uint32_t));

    if (heap_variable != NULL)
    {
        *heap_variable = 1951;
        printf("stack_variable  0x%08x  %u\n",
               (unsigned)(uintptr_t)&stack_variable, stack_variable);
        printf("heap_variable   0x%08x  %u\n",
               (unsigned)(uintptr_t)heap_variable, *heap_variable);
    }

    free(heap_variable);
}

#define VECTOR_TABLE 0x10000100
#define GPIO_IN_REG  0xd0000004

void boot_info(void)
{
    const uint32_t *vectors = (const uint32_t *)VECTOR_TABLE;

    uint32_t stack_top = vectors[0];
    uint32_t reset_handler = vectors[1];

    volatile uint32_t *gpio_in = (uint32_t *)GPIO_IN_REG;
    uint32_t level = (*gpio_in >> led_pin()) & 1u;

    printf("vector table   0x%08x\n", VECTOR_TABLE);
    printf("  stack top    0x%08x\n", (unsigned)stack_top);
    printf("  reset        0x%08x\n", (unsigned)reset_handler);
    printf("  reset (even) 0x%08x\n", (unsigned)(reset_handler & ~1u));
    printf("gpio in        0x%08x\n", GPIO_IN_REG);
    printf("  led bit      %u\n", (unsigned)level);
    printf("  gpio_get     %u\n", (unsigned)gpio_get(led_pin()));
}
