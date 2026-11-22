#include "stdio-text-protocol/stdio-text-protocol.h"

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

static char line_buf[STDIO_TEXT_PROTOCOL_LINE_SIZE];
static uint32_t line_len = 0;
static bool line_truncated = false;
static command_t parsed_command;

void stdio_text_protocol_init(void)
{
    line_len = 0;
    line_truncated = false;
}

static const command_t *stdio_text_protocol_finish_line(void)
{
    // завершаем строку
    line_buf[line_len] = '\0';

    parsed_command.name = NULL;
    parsed_command.argc = 0;
    parsed_command.truncated = line_truncated;

    for (uint32_t i = 0; i < STDIO_TEXT_PROTOCOL_MAX_ARGS; i++)
    {
        parsed_command.argv[i] = NULL;
    }

    // пустой кадр (только пробелы) не обрабатываем
    bool has_non_space = false;
    for (uint32_t i = 0; i < line_len; i++)
    {
        if (line_buf[i] != ' ')
        {
            has_non_space = true;
            break;
        }
    }
    if (!has_non_space)
    {
        line_len = 0;
        line_truncated = false;
        return NULL;
    }

    // делим строку на слова на месте
    char *p = line_buf;
    char *end = line_buf + line_len;
    while (p < end)
    {
        while (p < end && *p == ' ')
        {
            *p = '\0';
            p++;
        }
        if (p >= end)
        {
            break;
        }

        if (parsed_command.name == NULL)
        {
            parsed_command.name = p;
        }
        else if (parsed_command.argc < STDIO_TEXT_PROTOCOL_MAX_ARGS)
        {
            parsed_command.argv[parsed_command.argc++] = p;
        }
        else
        {
            parsed_command.truncated = true;
        }

        while (p < end && *p != ' ')
        {
            p++;
        }
    }

    line_len = 0;
    line_truncated = false;
    return &parsed_command;
}

const command_t *stdio_text_protocol_handle(void)
{
    int ch = getchar_timeout_us(0);
    if (ch == PICO_ERROR_TIMEOUT)
    {
        return NULL;
    }

    unsigned char c = (unsigned char)ch;

    if (c == '\r' || c == '\n')
    {
        if (line_len == 0 && !line_truncated)
        {
            return NULL;
        }
        printf("\r\n");
        return stdio_text_protocol_finish_line();
    }

    if (c == 0x08 || c == 0x7f)
    {
        if (line_len > 0)
        {
            line_len--;
            line_buf[line_len] = '\0';
            line_truncated = false;
            printf("\b \b");
        }
        return NULL;
    }

    if (c >= 0x20 && c <= 0x7e)
    {
        if (line_len < (STDIO_TEXT_PROTOCOL_LINE_SIZE - 1))
        {
            line_buf[line_len++] = (char)c;
            putchar_raw(c);
        }
        else
        {
            line_truncated = true;
        }
        return NULL;
    }

    return NULL;
}
