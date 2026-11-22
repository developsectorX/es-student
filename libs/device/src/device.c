#include "device/device.h"

#include <stddef.h>

#include "pico/stdlib.h"
#include "pico/unique_id.h"
#include "hardware/regs/addressmap.h"

#ifndef DEVICE_BOARD
#define DEVICE_BOARD "unknown"
#endif

void device_get_info(device_info_t *info)
{
    if (info == NULL)
    {
        return;
    }

    info->board = DEVICE_BOARD;
    pico_get_unique_board_id_string(info->serial, sizeof(info->serial));

    volatile uint32_t *chip_id = (volatile uint32_t *)(PPB_BASE + 0xed00);
    uint32_t value = *chip_id;
    info->chip_revision = value & 0xFu;
    info->chip_part = (value >> 4) & 0xFFFu;
    info->chip_manufacturer = (value >> 12) & 0x7FFu;

    info->sdk_version = PICO_SDK_VERSION_STRING;
}
