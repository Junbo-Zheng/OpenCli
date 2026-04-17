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
 * @file cli_client.c
 * @brief CLI remote client — local line editing, sends CMD/TAB to server.
 */

#include "cli_client.h"
#include "cli_protocol.h"
#include "cli_port.h"

#include <stdio.h>
#include <string.h>

/* ---- VT100 helpers (same as cli.c) ---- */

static void client_erase_to_eol(void)
{
    cli_port_puts("\033[K");
}

static void client_cursor_left(int n)
{
    char buf[16];

    if (n <= 0) {
        return;
    }

    snprintf(buf, sizeof(buf), "\033[%dD", n);
    cli_port_puts(buf);
}

static void client_cursor_right(int n)
{
    char buf[16];

    if (n <= 0) {
        return;
    }

    snprintf(buf, sizeof(buf), "\033[%dC", n);
    cli_port_puts(buf);
}

/* ---- Line editing ---- */

static void client_redraw(cli_client_t *c)
{
    cli_port_putchar('\r');
    client_erase_to_eol();
    cli_port_puts(c->prompt);

    for (size_t i = 0; i < c->len; i++) {
        cli_port_putchar(c->line[i]);
    }

    client_cursor_left((int)(c->len - c->cursor));
}

static void client_insert_char(cli_client_t *c, char ch)
{
    if (c->len >= CONFIG_CLI_MAX_LINE - 1) {
        return;
    }

    memmove(&c->line[c->cursor + 1], &c->line[c->cursor],
            c->len - c->cursor);
    c->line[c->cursor] = ch;
    c->len++;
    c->line[c->len] = '\0';

    for (size_t i = c->cursor; i < c->len; i++) {
        cli_port_putchar(c->line[i]);
    }

    c->cursor++;
    client_cursor_left((int)(c->len - c->cursor));
}

static void client_backspace(cli_client_t *c)
{
    if (c->cursor == 0) {
        return;
    }

    c->cursor--;
    memmove(&c->line[c->cursor], &c->line[c->cursor + 1],
            c->len - c->cursor - 1);
    c->len--;
    c->line[c->len] = '\0';

    client_cursor_left(1);

    for (size_t i = c->cursor; i < c->len; i++) {
        cli_port_putchar(c->line[i]);
    }

    cli_port_putchar(' ');
    client_cursor_left((int)(c->len - c->cursor) + 1);
}

static void client_replace_line(cli_client_t *c, const char *s)
{
    c->line[0] = '\0';
    strncpy(c->line, s, CONFIG_CLI_MAX_LINE - 1);
    c->line[CONFIG_CLI_MAX_LINE - 1] = '\0';
    c->len = strlen(c->line);
    c->cursor = c->len;
    client_redraw(c);
}

/* ---- History ---- */

static int client_hist_total(cli_client_t *c)
{
    return c->hist_count < CONFIG_CLI_MAX_HISTORY ? c->hist_count
                                                  : CONFIG_CLI_MAX_HISTORY;
}

static const char *client_hist_get(cli_client_t *c, int idx)
{
    int total = client_hist_total(c);
    int start;

    if (idx < 0 || idx >= total) {
        return NULL;
    }

    start = c->hist_count <= CONFIG_CLI_MAX_HISTORY
                ? 0
                : c->hist_count % CONFIG_CLI_MAX_HISTORY;
    return c->history[(start + idx) % CONFIG_CLI_MAX_HISTORY];
}

static void client_history_add(cli_client_t *c, const char *line)
{
    if (c->hist_count > 0) {
        int last = (c->hist_count - 1) % CONFIG_CLI_MAX_HISTORY;
        if (strcmp(c->history[last], line) == 0) {
            return;
        }
    }

    int idx = c->hist_count % CONFIG_CLI_MAX_HISTORY;
    strncpy(c->history[idx], line, CONFIG_CLI_MAX_LINE - 1);
    c->history[idx][CONFIG_CLI_MAX_LINE - 1] = '\0';
    c->hist_count++;
}

static void client_history_prev(cli_client_t *c)
{
    int total = client_hist_total(c);

    if (total == 0 || c->hist_idx <= 0) {
        return;
    }

    if (c->hist_idx == total) {
        strncpy(c->saved_line, c->line, CONFIG_CLI_MAX_LINE - 1);
        c->saved_line[CONFIG_CLI_MAX_LINE - 1] = '\0';
    }

    c->hist_idx--;
    client_replace_line(c, client_hist_get(c, c->hist_idx));
}

static void client_history_next(cli_client_t *c)
{
    int total = client_hist_total(c);

    if (c->hist_idx >= total) {
        return;
    }

    c->hist_idx++;

    if (c->hist_idx == total) {
        client_replace_line(c, c->saved_line);
    } else {
        client_replace_line(c, client_hist_get(c, c->hist_idx));
    }
}

/* ---- Protocol I/O ---- */

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

static int send_frame(cli_transport_t fd, uint8_t type,
                      const void *payload, uint16_t len)
{
    uint8_t frame[CLI_PROTO_HEADER_SIZE + CLI_PROTO_MAX_PAYLOAD];
    size_t total = cli_proto_encode(frame, type, payload, len);

    return cli_transport_send(fd, frame, total);
}

static int recv_frame(cli_transport_t fd, uint8_t *type,
                      char *payload, uint16_t *payload_len)
{
    uint8_t hdr[CLI_PROTO_HEADER_SIZE];

    if (recv_exact(fd, hdr, sizeof(hdr)) < 0) {
        return -1;
    }

    cli_proto_decode_header(hdr, type, payload_len);

    if (*payload_len > CLI_PROTO_MAX_PAYLOAD - 1) {
        return -1;
    }

    if (*payload_len > 0) {
        if (recv_exact(fd, payload, *payload_len) < 0) {
            return -1;
        }
    }

    payload[*payload_len] = '\0';
    return 0;
}

/* ---- Tab completion via server ---- */

static void client_tab_complete(cli_client_t *c)
{
    char input[CONFIG_CLI_MAX_LINE];
    char payload[CLI_PROTO_MAX_PAYLOAD];
    uint8_t type;
    uint16_t payload_len;

    /* Send text up to cursor */
    memcpy(input, c->line, c->cursor);
    input[c->cursor] = '\0';

    if (send_frame(c->fd, CLI_PROTO_MSG_TAB, input,
                   (uint16_t)c->cursor) < 0) {
        return;
    }

    if (recv_frame(c->fd, &type, payload, &payload_len) < 0) {
        return;
    }

    if (type != CLI_PROTO_MSG_COMP || payload_len == 0) {
        return;
    }

    /* Parse candidates (tab-separated) */
    int count = 0;
    const char *candidates[64];
    char *p = payload;

    while (*p && count < 64) {
        candidates[count++] = p;
        while (*p && *p != '\t') {
            p++;
        }

        if (*p == '\t') {
            *p++ = '\0';
        }
    }

    if (count == 0) {
        return;
    }

    /* Find current prefix */
    const char *prefix = input;
    const char *last_space = NULL;

    for (const char *s = input; *s; s++) {
        if (*s == ' ' || *s == '\t') {
            last_space = s;
        }
    }

    if (last_space) {
        prefix = last_space + 1;
    }

    size_t prefix_len = strlen(prefix);

    if (count == 1) {
        /* Single match — complete it */
        const char *tail = candidates[0] + prefix_len;
        while (*tail) {
            client_insert_char(c, *tail++);
        }

        client_insert_char(c, ' ');
    } else {
        /* Multiple matches — find common prefix and show candidates */
        size_t common = strlen(candidates[0]);

        for (int i = 1; i < count; i++) {
            size_t j = 0;
            while (j < common && candidates[0][j] == candidates[i][j]) {
                j++;
            }

            common = j;
        }

        if (common > prefix_len) {
            for (size_t i = prefix_len; i < common; i++) {
                client_insert_char(c, candidates[0][i]);
            }
        } else {
            cli_port_puts("\r\n");
            for (int i = 0; i < count; i++) {
                cli_port_puts(candidates[i]);
                cli_port_puts("  ");
            }

            cli_port_puts("\r\n");
            client_redraw(c);
        }
    }
}

/* ---- Escape sequence handling ---- */

static void client_handle_escape(cli_client_t *c)
{
    int c2 = cli_port_getchar();

    if (c2 == '[' || c2 == 'O') {
        int c3 = cli_port_getchar();

        if (c2 == '[') {
            while ((c3 >= '0' && c3 <= '9') || c3 == ';') {
                c3 = cli_port_getchar();
            }
        }

        switch (c3) {
        case 'A':
            client_history_prev(c);
            break;
        case 'B':
            client_history_next(c);
            break;
        case 'C':
            if (c->cursor < c->len) {
                client_cursor_right(1);
                c->cursor++;
            }
            break;
        case 'D':
            if (c->cursor > 0) {
                client_cursor_left(1);
                c->cursor--;
            }
            break;
        default:
            break;
        }
    }
}

/* ---- Public API ---- */

int cli_client_init(cli_client_t *client, const char *path,
                    const char *prompt)
{
    memset(client, 0, sizeof(*client));
    strncpy(client->prompt, prompt ? prompt : "cli> ",
            CONFIG_CLI_MAX_PROMPT - 1);
    client->fd = cli_transport_connect(
        path ? path : CONFIG_CLI_NET_SOCKET_PATH);

    if (client->fd == CLI_TRANSPORT_INVALID) {
        return -1;
    }

    return 0;
}

void cli_client_run(cli_client_t *client)
{
    client->running = true;

    cli_port_raw_mode();

    while (client->running) {
        int ch;

        /* Reset line */
        client->line[0] = '\0';
        client->len = 0;
        client->cursor = 0;
        client->hist_idx = client_hist_total(client);

        cli_port_puts(client->prompt);
        cli_port_flush();

        for (;;) {
            ch = cli_port_getchar();
            if (ch < 0) {
                client->running = false;
                break;
            }

            switch (ch) {
            case '\r':
            case '\n': {
                cli_port_puts("\r\n");
                cli_port_flush();

                if (client->len == 0) {
                    goto next_line;
                }

                client_history_add(client, client->line);

                /* Check local quit/exit */
                if (strcmp(client->line, "quit") == 0 ||
                    strcmp(client->line, "exit") == 0) {
                    client->running = false;
                    goto done;
                }

                /* Send CMD to server */
                if (send_frame(client->fd, CLI_PROTO_MSG_CMD,
                               client->line,
                               (uint16_t)client->len) < 0) {
                    cli_port_puts("Error: server disconnected\r\n");
                    client->running = false;
                    goto done;
                }

                /* Receive response */
                {
                    char rsp[CLI_PROTO_MAX_PAYLOAD];
                    uint8_t type;
                    uint16_t rsp_len;

                    if (recv_frame(client->fd, &type, rsp, &rsp_len) < 0) {
                        cli_port_puts("Error: server disconnected\r\n");
                        client->running = false;
                        goto done;
                    }

                    if (type == CLI_PROTO_MSG_RSP && rsp_len > 0) {
                        cli_port_puts(rsp);
                        cli_port_flush();
                    }
                }

                goto next_line;
            }

            case 4: /* Ctrl-D */
                if (client->len == 0) {
                    cli_port_puts("\r\n");
                    client->running = false;
                    goto done;
                }
                break;

            case 127: /* Backspace */
            case 8:
                client_backspace(client);
                break;

            case '\t':
                client_tab_complete(client);
                break;

            case 3: /* Ctrl-C */
                cli_port_puts("^C\r\n");
                client->running = false;
                goto done;

            case 27: /* ESC */
                client_handle_escape(client);
                break;

            default:
                if (ch >= 32 && ch < 127) {
                    client_insert_char(client, (char)ch);
                }
                break;
            }

            cli_port_flush();
        }

next_line:
        continue;
    }

done:
    cli_port_restore_mode();
}

void cli_client_deinit(cli_client_t *client)
{
    cli_transport_close(client->fd);
    client->fd = CLI_TRANSPORT_INVALID;
}
