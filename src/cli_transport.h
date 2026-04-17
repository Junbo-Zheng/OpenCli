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
 * @file cli_transport.h
 * @brief Transport port layer for CLI client/server communication.
 *
 * Abstracts the underlying IPC mechanism (Unix socket, POSIX mq, etc.).
 * Server calls bind/accept/recv/send; client calls connect/send/recv.
 */

#ifndef CLI_TRANSPORT_H
#define CLI_TRANSPORT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque transport handle. */
typedef int cli_transport_t;

#define CLI_TRANSPORT_INVALID (-1)

/**
 * @brief Create and bind a server transport endpoint.
 * @param path Endpoint address (socket path or mq name).
 * @return Server handle, or CLI_TRANSPORT_INVALID on error.
 */
cli_transport_t cli_transport_server_open(const char *path);

/**
 * @brief Accept a client connection (blocking).
 * @param server Server handle from cli_transport_server_open().
 * @return Client handle, or CLI_TRANSPORT_INVALID on error.
 */
cli_transport_t cli_transport_accept(cli_transport_t server);

/**
 * @brief Connect to a server endpoint.
 * @param path Endpoint address.
 * @return Client handle, or CLI_TRANSPORT_INVALID on error.
 */
cli_transport_t cli_transport_connect(const char *path);

/**
 * @brief Send data.
 * @return Bytes sent, or -1 on error.
 */
int cli_transport_send(cli_transport_t handle, const void *buf, size_t len);

/**
 * @brief Receive data (blocking).
 * @return Bytes received, 0 on disconnect, or -1 on error.
 */
int cli_transport_recv(cli_transport_t handle, void *buf, size_t len);

/**
 * @brief Close a transport handle.
 */
void cli_transport_close(cli_transport_t handle);

/**
 * @brief Close server and unlink endpoint.
 */
void cli_transport_server_close(cli_transport_t server, const char *path);

/**
 * @brief Get the raw fd/handle for use with poll().
 */
int cli_transport_get_fd(cli_transport_t handle);

#ifdef __cplusplus
}
#endif

#endif /* CLI_TRANSPORT_H */
