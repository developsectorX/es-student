#include "stdio-text-protocol/stdio-text-protocol.h"
#include <stdio.h>
#include "pico/stdlib.h"

static char line_buf[STDIO_TEXT_PROTOCOL_LINE_SIZE];
static uint32_t line_len = 0;
static command_t current_command;

void stdio_text_protocol_init(void)
{
    line_len = 0;
    current_command.name = NULL;
    current_command.argc = 0;
    current_command.truncated = false;
}

const command_t *stdio_text_protocol_handle(void)
{
    int symbol = getchar_timeout_us(0);

    if (symbol == PICO_ERROR_TIMEOUT)
    {
        return NULL;
    }

    // Handle Backspace (0x08 or 0x7f)
    if (symbol == 0x08 || symbol == 0x7f)
    {
        if (line_len > 0)
        {
            line_len--;
            printf("\b \b");
        }
        return NULL;
    }

    // End of line (Enter: \r or \n)
    if (symbol == '\r' || symbol == '\n')
    {
        putchar('\n');
        line_buf[line_len] = '\0';

        // If the line is empty, just reset and return nothing
        if (line_len == 0)
        {
            return NULL;
        }

        // Check the truncated flag (if the line overflowed during input)
        bool is_truncated = current_command.truncated;

        // Reset the buffer for the next line
        line_len = 0;
        current_command.truncated = false;

        if (is_truncated)
        {
            // FIXED: use the actual name from the buffer instead of an empty string
            current_command.name = line_buf;
            current_command.argc = 0;
            return &current_command;
        }

        // Parse the line into tokens (in-place whitespace splitting)
        char *p = line_buf;
        
        // Skip leading whitespace
        while (*p == ' ' || *p == '\t')
        {
            p++;
        }

        if (*p == '\0')
        {
            return NULL; // The line consisted only of whitespace
        }

        current_command.name = p;
        current_command.argc = 0;

        // Find the end of the command name
        while (*p != '\0' && *p != ' ' && *p != '\t')
        {
            p++;
        }

        if (*p != '\0')
        {
            *p = '\0'; // Replace the whitespace with a null terminator
            p++;

            // Parse arguments
            while (*p != '\0' && current_command.argc < STDIO_TEXT_PROTOCOL_MAX_ARGS)
            {
                // Skip whitespace between arguments
                while (*p == ' ' || *p == '\t')
                {
                    p++;
                }

                if (*p == '\0')
                {
                    break;
                }

                current_command.argv[current_command.argc++] = p;

                // Find the end of the current argument
                while (*p != '\0' && *p != ' ' && *p != '\t')
                {
                    p++;
                }

                if (*p != '\0')
                {
                    *p = '\0';
                    p++;
                }
            }

            // If there are unread tokens left (more arguments than MAX_ARGS)
            while (*p != '\0')
            {
                if (*p != ' ' && *p != '\t')
                {
                    current_command.truncated = true;
                    break;
                }
                p++;
            }
        }

        return &current_command;
    }

    // Printable character
    if (line_len + 1 < STDIO_TEXT_PROTOCOL_LINE_SIZE)
    {
        line_buf[line_len++] = (char)symbol;
        putchar(symbol);
    }
    else
    {
        // Buffer overflow
        current_command.truncated = true;
    }

    return NULL;
}