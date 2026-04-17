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
 * @file cli_transport_mq.c
 * @brief POSIX message queue transport implementation (RTOS).
 *
 * Uses a pair of mq: <path>_req (client->server) and <path>_rsp (server->client).
 * The transport handle encodes the two mq descriptors.
 *
 * TODO: Implement for your RTOS. This is a stub template.
 */

#include "cli_transport.h"

/* TODO: Implement POSIX mq transport for your RTOS.
 *
 * Typical approach:
 *   - server_open: mq_open(<path>_req, O_RDONLY|O_CREAT)
 *   - accept: mq_open(<path>_rsp, O_WRONLY|O_CREAT), return composite handle
 *   - connect: mq_open(<path>_req, O_WRONLY), mq_open(<path>_rsp, O_RDONLY)
 *   - send/recv: mq_send/mq_receive on the appropriate queue
 */

cli_transport_t cli_transport_server_open(const char *path)
{
    (void)path;
    return CLI_TRANSPORT_INVALID;
}

cli_transport_t cli_transport_accept(cli_transport_t server)
{
    (void)server;
    return CLI_TRANSPORT_INVALID;
}

cli_transport_t cli_transport_connect(const char *path)
{
    (void)path;
    return CLI_TRANSPORT_INVALID;
}

int cli_transport_send(cli_transport_t handle, const void *buf, size_t len)
{
    (void)handle;
    (void)buf;
    (void)len;
    return -1;
}

int cli_transport_recv(cli_transport_t handle, void *buf, size_t len)
{
    (void)handle;
    (void)buf;
    (void)len;
    return -1;
}

void cli_transport_close(cli_transport_t handle)
{
    (void)handle;
}

void cli_transport_server_close(cli_transport_t server, const char *path)
{
    (void)server;
    (void)path;
}

int cli_transport_get_fd(cli_transport_t handle)
{
    (void)handle;
    return -1;
}
