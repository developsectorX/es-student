#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    double value;
    uint64_t time_ms;
} pi_result_t;

bool pi_task_start(uint32_t terms);
void pi_task_handle(void);

bool pi_task_is_running(void);
bool pi_task_is_done(void);
bool pi_task_was_started(void);

bool pi_task_get_result(pi_result_t *result);
uint32_t pi_task_get_terms(void);
