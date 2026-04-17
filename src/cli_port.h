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
 * @file cli_port.h
 * @brief CLI porting layer interface.
 *
 * Implement these functions for your platform (UART, USB, socket, etc.).
 * The CLI core only calls these — it never touches hardware directly.
 *
 * See cli_port_linux.c for a Linux reference implementation.
 */

#ifndef CLI_PORT_H
#define CLI_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read one byte from the input (blocking).
 * @return Character read (0-255), or -1 on EOF/error.
 */
int cli_port_getchar(void);

/**
 * @brief Write one byte to the output.
 * @param c Character to write.
 */
void cli_port_putchar(char c);

/**
 * @brief Write a null-terminated string to the output.
 * @param s String to write.
 */
void cli_port_puts(const char *s);

/**
 * @brief Flush the output buffer.
 *
 * Can be a no-op if the platform doesn't buffer output.
 */
void cli_port_flush(void);

/**
 * @brief Enter raw mode (disable echo and line buffering).
 *
 * Called once before each readline. On RTOS with raw UART, this can be a no-op.
 */
void cli_port_raw_mode(void);

/**
 * @brief Restore normal terminal mode.
 *
 * Called after each readline completes. On RTOS, this can be a no-op.
 */
void cli_port_restore_mode(void);

#ifdef __cplusplus
}
#endif

#endif /* CLI_PORT_H */
