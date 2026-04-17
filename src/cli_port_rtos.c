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
 * @file cli_port_rtos.c
 * @brief CLI port layer — RTOS template (implement for your platform).
 */

#include "cli_port.h"

/* TODO: include your UART/BSP headers here */

int cli_port_getchar(void)
{
    /* TODO: blocking read 1 byte from UART */
    return -1;
}

void cli_port_putchar(char c)
{
    /* TODO: write 1 byte to UART */
    (void)c;
}

void cli_port_puts(const char *s)
{
    /* TODO: write string to UART */
    while (*s) {
        cli_port_putchar(*s++);
    }
}

void cli_port_flush(void)
{
    /* TODO: flush output buffer, or no-op if unbuffered */
}

void cli_port_raw_mode(void)
{
    /* no-op — RTOS serial is typically already raw */
}

void cli_port_restore_mode(void)
{
    /* no-op */
}
