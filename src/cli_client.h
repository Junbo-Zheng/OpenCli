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
 * @file cli_client.h
 * @brief CLI remote client — local line editing, sends CMD/TAB to server.
 *
 * The client handles all user interaction (line editing, history, display).
 * Tab completion and command execution are forwarded to the server.
 */

#ifndef CLI_CLIENT_H
#define CLI_CLIENT_H

#include "cli_transport.h"
#include "cli_config.h"

#include <stdbool.h>
#include <stddef.h>

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

#ifndef CONFIG_CLI_NET_SOCKET_PATH
#define CONFIG_CLI_NET_SOCKET_PATH "/tmp/cli.sock"
#endif

/** @brief Client instance. */
typedef struct {
    cli_transport_t fd;
    char prompt[CONFIG_CLI_MAX_PROMPT];

    /* Line buffer */
    char line[CONFIG_CLI_MAX_LINE];
    size_t len;
    size_t cursor;

    /* History */
    char history[CONFIG_CLI_MAX_HISTORY][CONFIG_CLI_MAX_LINE];
    int hist_count;
    int hist_idx;
    char saved_line[CONFIG_CLI_MAX_LINE];

    bool running;
} cli_client_t;

/**
 * @brief Initialize and connect client to server.
 * @param client Client instance.
 * @param path Server endpoint (NULL for default).
 * @param prompt Prompt string.
 * @return 0 on success, -1 on error.
 */
int cli_client_init(cli_client_t *client, const char *path,
                    const char *prompt);

/**
 * @brief Run client interactive loop (blocking).
 * @param client Client instance.
 */
void cli_client_run(cli_client_t *client);

/**
 * @brief Cleanup client resources.
 */
void cli_client_deinit(cli_client_t *client);

#ifdef __cplusplus
}
#endif

#endif /* CLI_CLIENT_H */
