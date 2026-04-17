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
 * @file cli.h
 * @brief Portable CLI core — line editing, tab completion, command history.
 *
 * Supports hierarchical sub-commands with multi-level tab completion.
 * Pure C, no dynamic allocation. Depends only on cli_port.h for I/O.
 */

#ifndef CLI_H
#define CLI_H

#include <stdbool.h>
#include <stddef.h>

#include "cli_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CONFIG_CLI_MAX_LINE
#define CONFIG_CLI_MAX_LINE 256
#endif

#ifndef CONFIG_CLI_MAX_HISTORY
#define CONFIG_CLI_MAX_HISTORY 16
#endif

#ifndef CONFIG_CLI_MAX_PROMPT
#define CONFIG_CLI_MAX_PROMPT 32
#endif

#ifndef CONFIG_CLI_MAX_COMMANDS
#define CONFIG_CLI_MAX_COMMANDS 32
#endif

#ifndef CONFIG_CLI_MAX_SUB_COMMANDS
#define CONFIG_CLI_MAX_SUB_COMMANDS 16
#endif

/**
 * @brief Command handler function type.
 * @param argc Argument count (remaining args after sub-command resolution).
 * @param argv Argument vector.
 */
typedef void (*cli_cmd_fn)(int argc, char *argv[]);

/** @brief Command node — supports sub-command tree. */
typedef struct cli_cmd {
    const char *name;
    const char *help;
    cli_cmd_fn handler;             /**< NULL if this is a group (has subs) */
    struct cli_cmd *subs;           /**< Sub-command array (static storage) */
    int sub_count;
    int sub_capacity;
} cli_cmd_t;

/** @brief CLI instance (all state, no globals). */
typedef struct {
    /* Prompt */
    char prompt[CONFIG_CLI_MAX_PROMPT];

    /* Line buffer */
    char line[CONFIG_CLI_MAX_LINE];
    size_t len;
    size_t cursor;

    /* History ring buffer */
    char history[CONFIG_CLI_MAX_HISTORY][CONFIG_CLI_MAX_LINE];
    int hist_count;
    int hist_idx;
    char saved_line[CONFIG_CLI_MAX_LINE];

    /* Top-level commands */
    cli_cmd_t cmds[CONFIG_CLI_MAX_COMMANDS];
    int cmd_count;

    /* Run state */
    bool running;

    /* Static pool for sub-command arrays */
    cli_cmd_t sub_pool[CONFIG_CLI_MAX_COMMANDS * CONFIG_CLI_MAX_SUB_COMMANDS];
    int sub_pool_used;
} cli_t;

/**
 * @brief Initialize CLI instance.
 */
void cli_init(cli_t *cli, const char *prompt);

/**
 * @brief Register a command (top-level or sub-command).
 * @param cli CLI instance.
 * @param parent Parent command node, or NULL for top-level.
 * @param name Command name.
 * @param help Help string.
 * @param handler Handler function, or NULL for group nodes.
 * @return Pointer to the command node (for adding sub-commands), or NULL.
 */
cli_cmd_t *cli_register(cli_t *cli, cli_cmd_t *parent, const char *name,
                         const char *help, cli_cmd_fn handler);

/**
 * @brief Read one line with editing, completion, and history.
 * @param cli CLI instance.
 * @param buf Output buffer for the line.
 * @param size Size of buf.
 * @return true on success, false on EOF (Ctrl-D).
 */
bool cli_readline(cli_t *cli, char *buf, size_t size);

/**
 * @brief Parse and execute a command line.
 * @param cli CLI instance.
 * @param line Command line string (will be modified in-place for tokenization).
 */
void cli_execute(cli_t *cli, char *line);

/**
 * @brief Main loop — readline + execute until EOF or quit.
 * @param cli CLI instance.
 */
void cli_run(cli_t *cli);

/**
 * @brief Signal CLI to exit the run loop.
 * @param cli CLI instance.
 */
void cli_quit(cli_t *cli);

#ifdef __cplusplus
}
#endif

#endif /* CLI_H */
