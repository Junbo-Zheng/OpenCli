/*
 * Copyright 2026 Junbo Zheng
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file cli_port_linux.c
 * @brief CLI port layer — Linux/macOS reference implementation using termios.
 *
 * For RTOS, replace this file with your UART-based implementation.
 */

#include "cli_port.h"

#include <stdio.h>
#include <stdbool.h>
#include <termios.h>
#include <unistd.h>

static struct termios orig_termios;
static bool raw_active = false;

/* Output hook for server-side output capture */
static void (*s_output_hook)(const char *);

void cli_server_set_output_hook(void (*fn)(const char *))
{
    s_output_hook = fn;
}

int cli_port_getchar(void)
{
    return getchar();
}

void cli_port_putchar(char c)
{
    if (s_output_hook) {
        char buf[2] = {c, '\0'};
        s_output_hook(buf);
        return;
    }

    putchar(c);
}

void cli_port_puts(const char *s)
{
    if (s_output_hook) {
        s_output_hook(s);
        return;
    }

    fputs(s, stdout);
}

void cli_port_flush(void)
{
    fflush(stdout);
}

void cli_port_raw_mode(void)
{
    struct termios raw;

    if (raw_active) {
        return;
    }

    tcgetattr(STDIN_FILENO, &orig_termios);
    raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    raw_active = true;
}

void cli_port_restore_mode(void)
{
    if (!raw_active) {
        return;
    }

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    raw_active = false;
}
