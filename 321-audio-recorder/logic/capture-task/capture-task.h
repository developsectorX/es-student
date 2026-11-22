// capture-task.h — задача захвата по будильнику: запускает захват отсчётов
// с фиксированным периодом и сообщает, когда он окончен.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "platform/platform.h"

// false — канал измерений занят или параметры вне диапазона
bool capture_task_start(uint32_t count, uint32_t period_us);

bool capture_task_done(void);

void capture_task_get(platform_capture_t *capture);
