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
    // Шапка таблицы
    printf("%-10s %-10s %-10s %8s\n", "region", "start", "end", "size");

    // --- Регионы, заданные чипом/платой ---
    row("flash", XIP_BASE, XIP_BASE + PICO_FLASH_SIZE_BYTES);
    row("sram", SRAM_BASE, SRAM_BASE + SRAM_SIZE);
    row("rom", ROM_BASE, ROM_BASE + ROM_SIZE);

    // --- Секции образа (символы компоновщика) ---
    row("image", (uintptr_t)&__flash_binary_start, (uintptr_t)&__flash_binary_end);
    row("free", (uintptr_t)&__flash_binary_end,
        XIP_BASE + PICO_FLASH_SIZE_BYTES);

    row("boot2", (uintptr_t)&__boot2_start__, (uintptr_t)&__boot2_end__);
    row("text", (uintptr_t)&__boot2_end__, (uintptr_t)&__etext);

    // .data хранится во флеше (начальные значения) и копируется в ОЗУ
    row("data flash", (uintptr_t)&__etext,
        (uintptr_t)&__etext + ((uintptr_t)&__data_end__ - (uintptr_t)&__data_start__));
    row("data ram", (uintptr_t)&__data_start__, (uintptr_t)&__data_end__);
    row("bss", (uintptr_t)&__bss_start__, (uintptr_t)&__bss_end__);
    row("heap", (uintptr_t)&__bss_end__, (uintptr_t)&__HeapLimit);
    row("stack", (uintptr_t)&__StackBottom, (uintptr_t)&__StackTop);

    // --- Итоги ---
    printf("\n");

    uintptr_t img_start = (uintptr_t)&__flash_binary_start;
    uintptr_t img_end = (uintptr_t)&__flash_binary_end;
    uintptr_t flash_end = XIP_BASE + PICO_FLASH_SIZE_BYTES;

    printf("image in flash: %u bytes\n", (unsigned)(img_end - img_start));
    printf("free in flash:  %u bytes of %u\n",
        (unsigned)(flash_end - img_end),
        (unsigned)PICO_FLASH_SIZE_BYTES);

    uintptr_t ram_used = ((uintptr_t)&__data_end__ - (uintptr_t)&__data_start__)
        + ((uintptr_t)&__bss_end__ - (uintptr_t)&__bss_start__);
    printf("ram used (.data + .bss): %u bytes\n", (unsigned)ram_used);

    uintptr_t ram_free = ((uintptr_t)&__HeapLimit - (uintptr_t)&__bss_end__)
        + ((uintptr_t)&__StackTop - (uintptr_t)&__StackBottom);
    printf("ram free (heap + stack): %u bytes\n", (unsigned)ram_free);
}
