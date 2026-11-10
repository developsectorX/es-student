#pragma once
#include <stdbool.h>
#include <stdint.h>

#define STDIO_TEXT_PROTOCOL_LINE_SIZE 64
#define STDIO_TEXT_PROTOCOL_MAX_ARGS 4

// Command parsed from a single line
typedef struct {
    const char *name;
    uint32_t argc;
    const char *argv[STDIO_TEXT_PROTOCOL_MAX_ARGS];

    // The line is longer than the buffer or there are more arguments than fit in argv:
    // the command was not received completely and must not be executed
    bool truncated;
} command_t;

void stdio_text_protocol_init(void);

// Reads one character without waiting for it. When a line is complete, returns
// a command, otherwise NULL. The command is valid until the next call.
const command_t *stdio_text_protocol_handle(void);