#pragma once
#include <stdbool.h>
#include <stdint.h>

#define STDIO_TEXT_PROTOCOL_LINE_SIZE 64
#define STDIO_TEXT_PROTOCOL_MAX_ARGS 4

typedef struct{
 bool truncated;
 uint32_t argc;
 const char *argv[STDIO_TEXT_PROTOCOL_MAX_ARGS];
 const char *name;
} command_t;

const command_t *stdio_text_protocol_handle(void);
void stdio_text_protocol_init(void);
