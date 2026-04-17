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
 * @file cli_server.c
 * @brief CLI server — multi-client poll loop, dispatches CMD/TAB to cli_t.
 */

#include "cli_server.h"
#include "cli_protocol.h"
#include "cli_port.h"

#include <poll.h>
#include <stdio.h>
#include <string.h>

/* ---- Output capture ---- */

/** Buffer for capturing cli_execute output. */
static char s_capture_buf[CLI_PROTO_MAX_PAYLOAD];
static size_t s_capture_len;

/**
 * Temporarily redirect cli_port_puts output to a buffer.
 * We save/restore the real port functions around execute.
 */
static void capture_reset(void)
{
    s_capture_buf[0] = '\0';
    s_capture_len = 0;
}

static void capture_puts(const char *s)
{
    size_t slen = strlen(s);
    size_t avail = sizeof(s_capture_buf) - s_capture_len - 1;

    if (slen > avail) {
        slen = avail;
    }

    memcpy(&s_capture_buf[s_capture_len], s, slen);
    s_capture_len += slen;
    s_capture_buf[s_capture_len] = '\0';
}

/* ---- Tab completion to buffer ---- */

/**
 * @brief Perform tab completion and return candidates as \t-separated string.
 *
 * Walks the command tree the same way tab_complete() does, but writes
 * candidates to a buffer instead of the terminal.
 */
static size_t tab_complete_to_buf(cli_t *cli, const char *input,
                                  char *out, size_t out_size)
{
    char tmp[CONFIG_CLI_MAX_LINE];
    size_t input_len = strlen(input);
    cli_cmd_t *group;
    int group_count;
    const char *prefix;
    size_t prefix_len;
    size_t pos = 0;

    if (input_len >= sizeof(tmp)) {
        input_len = sizeof(tmp) - 1;
    }

    memcpy(tmp, input, input_len);
    tmp[input_len] = '\0';

    group = cli->cmds;
    group_count = cli->cmd_count;
    prefix = tmp;

    /* Walk completed tokens to find the right sub-command group */
    char *p = tmp;
    while (*p) {
        while (*p == ' ' || *p == '\t') {
            p++;
        }

        if (*p == '\0') {
            break;
        }

        char *tok_start = p;
        while (*p && *p != ' ' && *p != '\t') {
            p++;
        }

        if (*p == '\0') {
            prefix = tok_start;
            break;
        }

        *p = '\0';
        bool found = false;
        for (int i = 0; i < group_count; i++) {
            if (strcmp(tok_start, group[i].name) == 0) {
                if (group[i].subs && group[i].sub_count > 0) {
                    group_count = group[i].sub_count;
                    group = group[i].subs;
                } else {
                    out[0] = '\0';
                    return 0;
                }

                found = true;
                break;
            }
        }

        if (!found) {
            out[0] = '\0';
            return 0;
        }

        p++;
        while (*p == ' ' || *p == '\t') {
            p++;
        }

        if (*p == '\0') {
            prefix = p;
            break;
        }
    }

    prefix_len = strlen(prefix);

    /* Collect matches */
    for (int i = 0; i < group_count; i++) {
        if (strncmp(group[i].name, prefix, prefix_len) == 0) {
            size_t nlen = strlen(group[i].name);
            if (pos + nlen + 1 >= out_size) {
                break;
            }

            if (pos > 0) {
                out[pos++] = '\t';
            }

            memcpy(&out[pos], group[i].name, nlen);
            pos += nlen;
        }
    }

    out[pos] = '\0';
    return pos;
}

/* ---- Protocol I/O helpers ---- */

/**
 * @brief Read exactly n bytes from transport.
 * @return 0 on success, -1 on disconnect/error.
 */
static int recv_exact(cli_transport_t fd, void *buf, size_t n)
{
    size_t got = 0;

    while (got < n) {
        int r = cli_transport_recv(fd, (char *)buf + got, n - got);
        if (r <= 0) {
            return -1;
        }

        got += (size_t)r;
    }

    return 0;
}

/**
 * @brief Send a full protocol frame.
 */
static int send_frame(cli_transport_t fd, uint8_t type,
                      const void *payload, uint16_t len)
{
    uint8_t frame[CLI_PROTO_HEADER_SIZE + CLI_PROTO_MAX_PAYLOAD];
    size_t total = cli_proto_encode(frame, type, payload, len);

    return cli_transport_send(fd, frame, total);
}

/* ---- Client handling ---- */

static void remove_client(cli_server_t *server, int idx)
{
    cli_transport_close(server->clients[idx]);
    server->clients[idx] = server->clients[server->client_count - 1];
    server->client_count--;
}

/**
 * @brief Handle one message from a client.
 * @return 0 to keep connection, -1 to disconnect.
 */
static int handle_client_msg(cli_server_t *server, int idx)
{
    uint8_t hdr[CLI_PROTO_HEADER_SIZE];
    uint8_t type;
    uint16_t payload_len;
    char payload[CLI_PROTO_MAX_PAYLOAD];

    if (recv_exact(server->clients[idx], hdr, sizeof(hdr)) < 0) {
        return -1;
    }

    cli_proto_decode_header(hdr, &type, &payload_len);

    if (payload_len > sizeof(payload) - 1) {
        return -1;
    }

    if (payload_len > 0) {
        if (recv_exact(server->clients[idx], payload, payload_len) < 0) {
            return -1;
        }
    }

    payload[payload_len] = '\0';

    switch (type) {
    case CLI_PROTO_MSG_CMD: {
        /* Execute command, capture output */
        capture_reset();

        /* Temporarily redirect cli_port_puts to our capture buffer.
         * We do this by saving the original and swapping — but cli_port_puts
         * is a compiled function, not a function pointer. So instead we
         * provide cli_execute_to_buf() which uses snprintf-style capture.
         *
         * Simpler approach: just call cli_execute and capture via a global
         * output hook. For now, we use a simple approach: fork the output
         * by overriding cli_port_puts at link time won't work for server.
         *
         * Best approach: add an output callback to cli_execute.
         * For now, we use the capture_puts approach with a global flag.
         */

        /* We need to redirect output. The cleanest way without modifying
         * cli.c's interface is to use the capture buffer approach.
         * cli.c calls cli_port_puts() — we'll set a global flag so that
         * cli_port_puts routes to our buffer when in server context. */

        extern void cli_server_set_output_hook(void (*fn)(const char *));
        cli_server_set_output_hook(capture_puts);
        cli_execute(server->cli, payload);
        cli_server_set_output_hook(NULL);

        send_frame(server->clients[idx], CLI_PROTO_MSG_RSP,
                   s_capture_buf, (uint16_t)s_capture_len);
        break;
    }

    case CLI_PROTO_MSG_TAB: {
        char comp_buf[CLI_PROTO_MAX_PAYLOAD];
        size_t comp_len;

        comp_len = tab_complete_to_buf(server->cli, payload,
                                       comp_buf, sizeof(comp_buf));
        send_frame(server->clients[idx], CLI_PROTO_MSG_COMP,
                   comp_buf, (uint16_t)comp_len);
        break;
    }

    default:
        break;
    }

    return 0;
}

/* ---- Signal handling for graceful shutdown ---- */

#include <signal.h>

static cli_server_t *s_signal_server;

static void signal_handler(int sig)
{
    (void)sig;
    if (s_signal_server) {
        s_signal_server->running = 0;
    }
}

/**
 * @brief Install SIGTERM/SIGINT handler for graceful server shutdown.
 * @param server Server instance (must remain valid while running).
 */
void cli_server_install_signal_handler(cli_server_t *server)
{
    s_signal_server = server;
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
}

/* ---- Public API ---- */

int cli_server_init(cli_server_t *server, cli_t *cli, const char *path)
{
    memset(server, 0, sizeof(*server));
    server->cli = cli;
    server->path = path ? path : CONFIG_CLI_NET_SOCKET_PATH;
    server->client_count = 0;

    for (int i = 0; i < CONFIG_CLI_NET_MAX_CLIENTS; i++) {
        server->clients[i] = CLI_TRANSPORT_INVALID;
    }

    server->listen_fd = cli_transport_server_open(server->path);
    if (server->listen_fd == CLI_TRANSPORT_INVALID) {
        return -1;
    }

    return 0;
}

void cli_server_run(cli_server_t *server)
{
    struct pollfd fds[1 + CONFIG_CLI_NET_MAX_CLIENTS];
    int nfds;

    server->running = 1;

    while (server->running) {
        nfds = 0;

        /* Listen socket */
        fds[nfds].fd = cli_transport_get_fd(server->listen_fd);
        fds[nfds].events = POLLIN;
        nfds++;

        /* Client sockets */
        for (int i = 0; i < server->client_count; i++) {
            fds[nfds].fd = cli_transport_get_fd(server->clients[i]);
            fds[nfds].events = POLLIN;
            nfds++;
        }

        int ret = poll(fds, (unsigned int)nfds, 1000);
        if (ret < 0) {
            break;
        }

        if (ret == 0) {
            continue;
        }

        /* Check listen socket for new connections */
        if (fds[0].revents & POLLIN) {
            if (server->client_count < CONFIG_CLI_NET_MAX_CLIENTS) {
                cli_transport_t client = cli_transport_accept(server->listen_fd);
                if (client != CLI_TRANSPORT_INVALID) {
                    server->clients[server->client_count++] = client;
                }
            }
        }

        /* Check client sockets */
        for (int i = server->client_count - 1; i >= 0; i--) {
            if (fds[1 + i].revents & (POLLIN | POLLHUP | POLLERR)) {
                if (fds[1 + i].revents & POLLIN) {
                    if (handle_client_msg(server, i) < 0) {
                        remove_client(server, i);
                    }
                } else {
                    remove_client(server, i);
                }
            }
        }
    }
}

void cli_server_stop(cli_server_t *server)
{
    server->running = 0;
}

void cli_server_deinit(cli_server_t *server)
{
    for (int i = 0; i < server->client_count; i++) {
        cli_transport_close(server->clients[i]);
    }

    server->client_count = 0;
    cli_transport_server_close(server->listen_fd, server->path);
    server->listen_fd = CLI_TRANSPORT_INVALID;
}
