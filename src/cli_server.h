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
 * @file cli_server.h
 * @brief CLI server — embeds into the main process, serves remote clients.
 *
 * The server owns a cli_t instance. Remote clients send CMD/TAB requests
 * over the transport layer; the server executes commands and returns output.
 */

#ifndef CLI_SERVER_H
#define CLI_SERVER_H

#include "cli.h"
#include "cli_transport.h"
#include "cli_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CONFIG_CLI_NET_MAX_CLIENTS
#define CONFIG_CLI_NET_MAX_CLIENTS 4
#endif

#ifndef CONFIG_CLI_NET_SOCKET_PATH
#define CONFIG_CLI_NET_SOCKET_PATH "/tmp/cli.sock"
#endif

/** @brief Server instance. */
typedef struct {
    cli_t *cli;                                         /**< Shared CLI core */
    cli_transport_t listen_fd;                          /**< Listening socket */
    cli_transport_t clients[CONFIG_CLI_NET_MAX_CLIENTS]; /**< Connected fds */
    int client_count;
    const char *path;                                   /**< Endpoint path */
    volatile int running;
} cli_server_t;

/**
 * @brief Initialize server.
 * @param server Server instance.
 * @param cli CLI core (already initialized with commands registered).
 * @param path Socket/mq path (NULL for default).
 * @return 0 on success, -1 on error.
 */
int cli_server_init(cli_server_t *server, cli_t *cli, const char *path);

/**
 * @brief Run server poll loop (blocking). Call from main thread.
 * @param server Server instance.
 */
void cli_server_run(cli_server_t *server);

/**
 * @brief Stop the server.
 */
void cli_server_stop(cli_server_t *server);

/**
 * @brief Cleanup server resources.
 */
void cli_server_deinit(cli_server_t *server);

/**
 * @brief Install SIGTERM/SIGINT handler for graceful shutdown.
 *
 * Without this, killing the server while blocked in poll() causes ASan
 * to report DEADLYSIGNAL because it cannot cleanly unwind the stack
 * from inside a syscall. The handler sets server->running = 0 so
 * cli_server_run() exits normally on the next poll() iteration.
 *
 * @param server Server instance (must remain valid while running).
 */
void cli_server_install_signal_handler(cli_server_t *server);

#ifdef __cplusplus
}
#endif

#endif /* CLI_SERVER_H */
