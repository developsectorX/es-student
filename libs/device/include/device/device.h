#pragma once
#include <stdint.h>

typedef struct{
 const char *sdk_version;
 uint32_t chip_revision;
 uint32_t chip_part;
 uint32_t chip_manufacturer;
 char serial[17];
 const char *board;
} device_info_t;

void device_get_info(device_info_t *info);
