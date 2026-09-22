#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/regs/addressmap.h"
#include <stdint.h>

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

