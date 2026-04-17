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
 * @file cli_transport_unix.c
 * @brief Unix domain socket transport implementation.
 */

#include "cli_transport.h"

#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

cli_transport_t cli_transport_server_open(const char *path)
{
    struct sockaddr_un addr;
    int fd;

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return CLI_TRANSPORT_INVALID;
    }

    unlink(path);

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return CLI_TRANSPORT_INVALID;
    }

    if (listen(fd, 4) < 0) {
        close(fd);
        unlink(path);
        return CLI_TRANSPORT_INVALID;
    }

    return fd;
}

cli_transport_t cli_transport_accept(cli_transport_t server)
{
    return accept(server, NULL, NULL);
}

cli_transport_t cli_transport_connect(const char *path)
{
    struct sockaddr_un addr;
    int fd;

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return CLI_TRANSPORT_INVALID;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return CLI_TRANSPORT_INVALID;
    }

    return fd;
}

int cli_transport_send(cli_transport_t handle, const void *buf, size_t len)
{
    return (int)write(handle, buf, len);
}

int cli_transport_recv(cli_transport_t handle, void *buf, size_t len)
{
    return (int)read(handle, buf, len);
}

void cli_transport_close(cli_transport_t handle)
{
    if (handle >= 0) {
        close(handle);
    }
}

void cli_transport_server_close(cli_transport_t server, const char *path)
{
    cli_transport_close(server);
    if (path) {
        unlink(path);
    }
}

int cli_transport_get_fd(cli_transport_t handle)
{
    return handle;
}
