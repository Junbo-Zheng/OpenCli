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
 * @file cli.c
 * @brief CLI core implementation — pure C, zero dynamic allocation.
 */

#include "cli.h"
#include "cli_port.h"

#include <stdio.h>
#include <string.h>

/* ---- VT100 helpers ---- */

static void cli_erase_to_eol(void)
{
    cli_port_puts("\033[K");
}

static void cli_cursor_left(int n)
{
    char buf[16];

    if (n <= 0) {
        return;
    }

    snprintf(buf, sizeof(buf), "\033[%dD", n);
    cli_port_puts(buf);
}

static void cli_cursor_right(int n)
{
    char buf[16];

    if (n <= 0) {
        return;
    }

    snprintf(buf, sizeof(buf), "\033[%dC", n);
    cli_port_puts(buf);
}

/* ---- Internal line operations ---- */

static void redraw(cli_t *cli)
{
    int back;

    cli_port_putchar('\r');
    cli_erase_to_eol();
    cli_port_puts(cli->prompt);

    for (size_t i = 0; i < cli->len; i++) {
        cli_port_putchar(cli->line[i]);
    }

    back = (int)(cli->len - cli->cursor);
    cli_cursor_left(back);
}

static void insert_char(cli_t *cli, char c)
{
    int back;

    if (cli->len >= CONFIG_CLI_MAX_LINE - 1) {
        return;
    }

    memmove(&cli->line[cli->cursor + 1], &cli->line[cli->cursor],
            cli->len - cli->cursor);
    cli->line[cli->cursor] = c;
    cli->len++;
    cli->line[cli->len] = '\0';

    for (size_t i = cli->cursor; i < cli->len; i++) {
        cli_port_putchar(cli->line[i]);
    }

    cli->cursor++;
    back = (int)(cli->len - cli->cursor);
    cli_cursor_left(back);
}

static void backspace(cli_t *cli)
{
    if (cli->cursor == 0) {
        return;
    }

    cli->cursor--;
    memmove(&cli->line[cli->cursor], &cli->line[cli->cursor + 1],
            cli->len - cli->cursor - 1);
    cli->len--;
    cli->line[cli->len] = '\0';

    cli_cursor_left(1);

    for (size_t i = cli->cursor; i < cli->len; i++) {
        cli_port_putchar(cli->line[i]);
    }

    cli_port_putchar(' ');
    cli_cursor_left((int)(cli->len - cli->cursor) + 1);
}

static void delete_char(cli_t *cli)
{
    if (cli->cursor >= cli->len) {
        return;
    }

    memmove(&cli->line[cli->cursor], &cli->line[cli->cursor + 1],
            cli->len - cli->cursor - 1);
    cli->len--;
    cli->line[cli->len] = '\0';

    for (size_t i = cli->cursor; i < cli->len; i++) {
        cli_port_putchar(cli->line[i]);
    }

    cli_port_putchar(' ');
    cli_cursor_left((int)(cli->len - cli->cursor) + 1);
}

static void replace_line(cli_t *cli, const char *s)
{
    cli->line[0] = '\0';
    strncpy(cli->line, s, CONFIG_CLI_MAX_LINE - 1);
    cli->line[CONFIG_CLI_MAX_LINE - 1] = '\0';
    cli->len = strlen(cli->line);
    cli->cursor = cli->len;
    redraw(cli);
}

/* ---- History ---- */

static void history_add(cli_t *cli, const char *line)
{
    int idx;

    if (cli->hist_count > 0) {
        int last = (cli->hist_count - 1) % CONFIG_CLI_MAX_HISTORY;
        if (strcmp(cli->history[last], line) == 0) {
            return;
        }
    }

    idx = cli->hist_count % CONFIG_CLI_MAX_HISTORY;
    strncpy(cli->history[idx], line, CONFIG_CLI_MAX_LINE - 1);
    cli->history[idx][CONFIG_CLI_MAX_LINE - 1] = '\0';
    cli->hist_count++;
}

static int hist_total(cli_t *cli)
{
    return cli->hist_count < CONFIG_CLI_MAX_HISTORY ? cli->hist_count
                                                    : CONFIG_CLI_MAX_HISTORY;
}

static const char *hist_get(cli_t *cli, int idx)
{
    int total = hist_total(cli);
    int start;
    int real;

    if (idx < 0 || idx >= total) {
        return NULL;
    }

    start = cli->hist_count <= CONFIG_CLI_MAX_HISTORY
                ? 0
                : cli->hist_count % CONFIG_CLI_MAX_HISTORY;
    real = (start + idx) % CONFIG_CLI_MAX_HISTORY;
    return cli->history[real];
}

static void history_prev(cli_t *cli)
{
    int total = hist_total(cli);

    if (total == 0 || cli->hist_idx <= 0) {
        return;
    }

    if (cli->hist_idx == total) {
        strncpy(cli->saved_line, cli->line, CONFIG_CLI_MAX_LINE - 1);
        cli->saved_line[CONFIG_CLI_MAX_LINE - 1] = '\0';
    }

    cli->hist_idx--;
    replace_line(cli, hist_get(cli, cli->hist_idx));
}

static void history_next(cli_t *cli)
{
    int total = hist_total(cli);

    if (cli->hist_idx >= total) {
        return;
    }

    cli->hist_idx++;

    if (cli->hist_idx == total) {
        replace_line(cli, cli->saved_line);
    } else {
        replace_line(cli, hist_get(cli, cli->hist_idx));
    }
}

/* ---- Tab completion (multi-level) ---- */

/**
 * Find the command group and prefix for completion.
 * Walk the token list to resolve sub-command levels.
 *
 * Example: "start ti|" -> group = start's subs, prefix = "ti"
 *          "st|"       -> group = top-level cmds, prefix = "st"
 */
static void tab_complete(cli_t *cli)
{
    char tmp[CONFIG_CLI_MAX_LINE];
    const char *matches[CONFIG_CLI_MAX_COMMANDS];
    int match_count = 0;
    cli_cmd_t *group;
    int group_count;
    const char *prefix;
    size_t prefix_len;

    /* Work on text up to cursor */
    memcpy(tmp, cli->line, cli->cursor);
    tmp[cli->cursor] = '\0';

    /* Start from top-level commands */
    group = cli->cmds;
    group_count = cli->cmd_count;
    prefix = tmp;

    /* Walk completed tokens to find the right sub-command group */
    char *p = tmp;
    while (*p) {
        /* Skip leading spaces */
        while (*p == ' ' || *p == '\t') {
            p++;
        }

        if (*p == '\0') {
            break;
        }

        /* Find end of this token */
        char *tok_start = p;
        while (*p && *p != ' ' && *p != '\t') {
            p++;
        }

        if (*p == '\0') {
            /* This is the incomplete token being typed — this is our prefix */
            prefix = tok_start;
            break;
        }

        /* Complete token — try to match in current group */
        *p = '\0';
        bool found = false;
        for (int i = 0; i < group_count; i++) {
            if (strcmp(tok_start, group[i].name) == 0) {
                /* Matched — descend into sub-commands if available */
                if (group[i].subs && group[i].sub_count > 0) {
                    group_count = group[i].sub_count;
                    group = group[i].subs;
                } else {
                    /* Leaf command, no more completion */
                    return;
                }

                found = true;
                break;
            }
        }

        if (!found) {
            return; /* Unknown token, can't complete */
        }

        p++; /* Skip past the null we wrote */

        /* If we're at the end after spaces, prefix is empty */
        while (*p == ' ' || *p == '\t') {
            p++;
        }

        if (*p == '\0') {
            /* Cursor is after a space — complete from current group with empty prefix */
            prefix = p;
            break;
        }
    }

    prefix_len = strlen(prefix);

    /* Find matches in current group */
    for (int i = 0; i < group_count; i++) {
        if (strncmp(group[i].name, prefix, prefix_len) == 0) {
            matches[match_count++] = group[i].name;
        }
    }

    if (match_count == 0) {
        return;
    }

    if (match_count == 1) {
        /* Single match — complete it */
        const char *tail = matches[0] + prefix_len;
        while (*tail) {
            insert_char(cli, *tail++);
        }

        /* Add space: if it has sub-commands, user will type next level;
         * if it's a leaf, user will type arguments */
        insert_char(cli, ' ');
        return;
    }

    /* Multiple matches — find common prefix */
    size_t common = strlen(matches[0]);

    for (int i = 1; i < match_count; i++) {
        size_t j = 0;
        while (j < common && matches[0][j] == matches[i][j]) {
            j++;
        }

        common = j;
    }

    if (common > prefix_len) {
        for (size_t i = prefix_len; i < common; i++) {
            insert_char(cli, matches[0][i]);
        }
    } else {
        /* Show all candidates */
        cli_port_putchar('\r');
        cli_port_putchar('\n');

        for (int i = 0; i < match_count; i++) {
            cli_port_puts(matches[i]);
            cli_port_puts("  ");
        }

        cli_port_putchar('\r');
        cli_port_putchar('\n');
        redraw(cli);
    }
}

/* ---- Escape sequence handling ---- */

static void handle_escape(cli_t *cli)
{
    int c2 = cli_port_getchar();

    if (c2 == '[' || c2 == 'O') {
        int c3 = cli_port_getchar();

        if (c2 == '[') {
            int param = 0;
            while ((c3 >= '0' && c3 <= '9') || c3 == ';') {
                if (c3 != ';') {
                    param = param * 10 + (c3 - '0');
                }

                c3 = cli_port_getchar();
            }

            if (c3 == '~') {
                if (param == 3) {
                    delete_char(cli);
                }

                return;
            }
        }

        switch (c3) {
        case 'A':
            history_prev(cli);
            break;
        case 'B':
            history_next(cli);
            break;
        case 'C':
            if (cli->cursor < cli->len) {
                cli_cursor_right(1);
                cli->cursor++;
            }
            break;
        case 'D':
            if (cli->cursor > 0) {
                cli_cursor_left(1);
                cli->cursor--;
            }
            break;
        case 'H':
            cli_cursor_left((int)cli->cursor);
            cli->cursor = 0;
            break;
        case 'F':
            cli_cursor_right((int)(cli->len - cli->cursor));
            cli->cursor = cli->len;
            break;
        default:
            break;
        }
    }
}

/* ---- Public API ---- */

void cli_init(cli_t *cli, const char *prompt)
{
    memset(cli, 0, sizeof(*cli));
    strncpy(cli->prompt, prompt, CONFIG_CLI_MAX_PROMPT - 1);
}

cli_cmd_t *cli_register(cli_t *cli, cli_cmd_t *parent, const char *name,
                         const char *help, cli_cmd_fn handler)
{
    cli_cmd_t *cmd;

    if (parent == NULL) {
        /* Top-level command */
        if (cli->cmd_count >= CONFIG_CLI_MAX_COMMANDS) {
            return NULL;
        }

        cmd = &cli->cmds[cli->cmd_count++];
    } else {
        /* Sub-command */
        int pool_max = CONFIG_CLI_MAX_COMMANDS * CONFIG_CLI_MAX_SUB_COMMANDS;

        if (parent->subs == NULL) {
            if (cli->sub_pool_used + CONFIG_CLI_MAX_SUB_COMMANDS > pool_max) {
                return NULL;
            }

            parent->subs = &cli->sub_pool[cli->sub_pool_used];
            parent->sub_capacity = CONFIG_CLI_MAX_SUB_COMMANDS;
            cli->sub_pool_used += CONFIG_CLI_MAX_SUB_COMMANDS;
        }

        if (parent->sub_count >= parent->sub_capacity) {
            return NULL;
        }

        cmd = &parent->subs[parent->sub_count++];
    }

    memset(cmd, 0, sizeof(*cmd));
    cmd->name = name;
    cmd->help = help;
    cmd->handler = handler;
    return cmd;
}

bool cli_readline(cli_t *cli, char *buf, size_t size)
{
    int c;

    cli_port_raw_mode();

    cli->line[0] = '\0';
    cli->len = 0;
    cli->cursor = 0;
    cli->hist_idx = hist_total(cli);

    cli_port_puts(cli->prompt);
    cli_port_flush();

    for (;;) {
        c = cli_port_getchar();
        if (c < 0) {
            cli_port_putchar('\r');
            cli_port_putchar('\n');
            cli_port_flush();
            cli_port_restore_mode();
            return false;
        }

        switch (c) {
        case '\r':
        case '\n':
            cli_port_putchar('\r');
            cli_port_putchar('\n');
            cli_port_flush();
            strncpy(buf, cli->line, size - 1);
            buf[size - 1] = '\0';
            if (cli->len > 0) {
                history_add(cli, cli->line);
            }

            cli_port_restore_mode();
            return true;

        case 4: /* Ctrl-D */
            if (cli->len == 0) {
                cli_port_putchar('\r');
                cli_port_putchar('\n');
                cli_port_flush();
                cli_port_restore_mode();
                return false;
            }

            delete_char(cli);
            break;

        case 127: /* Backspace */
        case 8:   /* Ctrl-H */
            backspace(cli);
            break;

        case '\t':
            tab_complete(cli);
            break;

        case 1: /* Ctrl-A */
            cli_cursor_left((int)cli->cursor);
            cli->cursor = 0;
            break;

        case 5: /* Ctrl-E */
            cli_cursor_right((int)(cli->len - cli->cursor));
            cli->cursor = cli->len;
            break;

        case 11: /* Ctrl-K */
            cli_erase_to_eol();
            cli->line[cli->cursor] = '\0';
            cli->len = cli->cursor;
            break;

        case 21: /* Ctrl-U */
            cli_port_putchar('\r');
            cli_erase_to_eol();
            cli_port_puts(cli->prompt);
            cli->line[0] = '\0';
            cli->len = 0;
            cli->cursor = 0;
            break;

        case 12: /* Ctrl-L */
            cli_port_puts("\033[2J\033[H");
            redraw(cli);
            break;

        case 3: /* Ctrl-C */
#if defined(CONFIG_CLI_CTRL_C_QUIT)
            cli_port_putchar('\r');
            cli_port_putchar('\n');
            cli_port_flush();
            cli_port_restore_mode();
            cli->running = false;
            buf[0] = '\0';
            return true;
#elif defined(CONFIG_CLI_CTRL_C_CLEAR_LINE)
            cli_port_puts("^C\r\n");
            cli->line[0] = '\0';
            cli->len = 0;
            cli->cursor = 0;
            cli_port_puts(cli->prompt);
            break;
#else
            break; /* ignore */
#endif

        case 27: /* ESC */
            handle_escape(cli);
            break;

        default:
            if (c >= 32 && c < 127) {
                insert_char(cli, (char)c);
            }
            break;
        }

        cli_port_flush();
    }
}

void cli_execute(cli_t *cli, char *line)
{
    char *argv[CONFIG_CLI_MAX_LINE / 2];
    int argc = 0;
    char *p = line;
    cli_cmd_t *group;
    int group_count;
    int consumed;

    /* Tokenize */
    while (*p && argc < (int)(sizeof(argv) / sizeof(argv[0])) - 1) {
        while (*p == ' ' || *p == '\t') {
            p++;
        }

        if (*p == '\0') {
            break;
        }

        argv[argc++] = p;

        while (*p && *p != ' ' && *p != '\t') {
            p++;
        }

        if (*p) {
            *p++ = '\0';
        }
    }

    if (argc == 0) {
        return;
    }

    /* Walk the command tree: consume tokens as long as they match sub-commands */
    group = cli->cmds;
    group_count = cli->cmd_count;
    consumed = 0;

    for (;;) {
        bool found = false;

        for (int i = 0; i < group_count; i++) {
            if (strcmp(argv[consumed], group[i].name) != 0) {
                continue;
            }

            consumed++;

            /* If this node has sub-commands and there are more tokens,
             * try to descend */
            if (group[i].subs && group[i].sub_count > 0 &&
                consumed < argc) {
                /* Check if next token matches a sub-command */
                for (int j = 0; j < group[i].sub_count; j++) {
                    if (strcmp(argv[consumed], group[i].subs[j].name) == 0) {
                        group_count = group[i].sub_count;
                        group = group[i].subs;
                        found = true;
                        break;
                    }
                }

                if (found) {
                    break; /* Continue outer for loop at deeper level */
                }
            }

            /* Execute this node */
            if (group[i].handler) {
                group[i].handler(argc - consumed + 1, &argv[consumed - 1]);
            } else if (group[i].subs && group[i].sub_count > 0) {
                /* Group without handler — show sub-commands */
                cli_port_puts("Sub-commands of '");
                cli_port_puts(group[i].name);
                cli_port_puts("':\r\n");
                for (int j = 0; j < group[i].sub_count; j++) {
                    cli_port_puts("  ");
                    cli_port_puts(group[i].subs[j].name);
                    if (group[i].subs[j].help) {
                        cli_port_puts(" — ");
                        cli_port_puts(group[i].subs[j].help);
                    }

                    cli_port_puts("\r\n");
                }
            } else {
                cli_port_puts("Unknown command: ");
                cli_port_puts(group[i].name);
                cli_port_puts("\r\n");
            }

            return;
        }

        if (!found) {
            cli_port_puts("Unknown command: ");
            cli_port_puts(argv[consumed]);
            cli_port_puts(" (type 'help' for commands)\r\n");
            return;
        }
    }
}

void cli_run(cli_t *cli)
{
    char buf[CONFIG_CLI_MAX_LINE];

    cli->running = true;

    while (cli->running && cli_readline(cli, buf, sizeof(buf))) {
        if (buf[0] == '\0') {
            continue;
        }

        cli_execute(cli, buf);
    }
}

void cli_quit(cli_t *cli)
{
    cli->running = false;
}
