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
 * @file main.c
 * @brief CLI demo — sub-command tree with multi-level tab completion.
 */

#include "cli.h"
#include "cli_port.h"

#include <stdio.h>
#include <string.h>

static cli_t cli;

/* ---- Built-in commands ---- */

#ifdef CONFIG_CLI_CMD_HELP
static void show_cmds(cli_cmd_t *cmds, int count, const char *prefix)
{
    for (int i = 0; i < count; i++) {
        char path[128];

        if (prefix[0]) {
            snprintf(path, sizeof(path), "%s %s", prefix, cmds[i].name);
        } else {
            snprintf(path, sizeof(path), "%s", cmds[i].name);
        }

        if (cmds[i].handler || !cmds[i].subs) {
            /* Leaf command — show full path + help */
            cli_port_puts("  ");
            cli_port_puts(path);

            int pad = 28 - (int)strlen(path);
            while (pad-- > 0) {
                cli_port_putchar(' ');
            }

            if (cmds[i].help) {
                cli_port_puts(cmds[i].help);
            }

            cli_port_puts("\r\n");
        } else {
            /* Group — show header then recurse */
            cli_port_puts("  ");
            cli_port_puts(path);
            cli_port_puts(" ...\r\n");
        }

        if (cmds[i].subs && cmds[i].sub_count > 0) {
            show_cmds(cmds[i].subs, cmds[i].sub_count, path);
        }
    }
}

static void cmd_help(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    cli_port_puts("Available commands:\r\n");
    show_cmds(cli.cmds, cli.cmd_count, "");
}
#endif

#ifdef CONFIG_CLI_CMD_VERSION
static void cmd_version(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    cli_port_puts("mini-cli v" CONFIG_CLI_VERSION "\r\n");
}
#endif

#ifdef CONFIG_CLI_CMD_ECHO
static void cmd_echo(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {
        if (i > 1) {
            cli_port_putchar(' ');
        }

        cli_port_puts(argv[i]);
    }

    cli_port_puts("\r\n");
}
#endif

#ifdef CONFIG_CLI_CMD_CLEAR
static void cmd_clear(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    cli_port_puts("\033[2J\033[H");
}
#endif

static void cmd_quit(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    cli_quit(&cli);
}

/* ---- Demo sub-commands: start/stop timer/log ---- */

static void cmd_start_timer(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    cli_port_puts("Timer started.\r\n");
}

static void cmd_start_log(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    cli_port_puts("Logging started.\r\n");
}

static void cmd_stop_timer(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    cli_port_puts("Timer stopped.\r\n");
}

static void cmd_stop_log(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    cli_port_puts("Logging stopped.\r\n");
}

/* ---- System info commands ---- */

/* 3-level: net if <up|down|status>, net dns <set|get> */

static void cmd_net_if_up(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    cli_port_puts("Interface eth0 up.\r\n");
}

static void cmd_net_if_down(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    cli_port_puts("Interface eth0 down.\r\n");
}

static void cmd_net_if_status(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    cli_port_puts("eth0: UP 192.168.1.100/24 mtu 1500\r\n");
}

static void cmd_net_dns_set(int argc, char *argv[])
{
    if (argc > 1) {
        cli_port_puts("DNS set to ");
        cli_port_puts(argv[1]);
        cli_port_puts("\r\n");
    } else {
        cli_port_puts("Usage: net dns set <ip>\r\n");
    }
}

static void cmd_net_dns_get(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    cli_port_puts("DNS: 8.8.8.8\r\n");
}

/* 4-level: sys config wifi <ssid|password|save> */

static void cmd_sys_cfg_wifi_ssid(int argc, char *argv[])
{
    if (argc > 1) {
        cli_port_puts("SSID set to ");
        cli_port_puts(argv[1]);
        cli_port_puts("\r\n");
    } else {
        cli_port_puts("Usage: sys config wifi ssid <name>\r\n");
    }
}

static void cmd_sys_cfg_wifi_pass(int argc, char *argv[])
{
    (void)argv;

    if (argc > 1) {
        cli_port_puts("Password set.\r\n");
    } else {
        cli_port_puts("Usage: sys config wifi password <pass>\r\n");
    }
}

static void cmd_sys_cfg_wifi_save(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    cli_port_puts("WiFi config saved to flash.\r\n");
}

/* ---- System commands — execute real system commands ---- */

static void run_system_cmd(const char *cmd)
{
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        cli_port_puts("Failed to execute command\r\n");
        return;
    }

    char buf[256];
    while (fgets(buf, sizeof(buf), fp)) {
        /* Replace \n with \r\n for terminal */
        char *nl = strchr(buf, '\n');
        if (nl) {
            *nl = '\0';
        }

        cli_port_puts(buf);
        cli_port_puts("\r\n");
    }

    pclose(fp);
}

#ifdef CONFIG_CLI_CMD_LS
static void cmd_ls(int argc, char *argv[])
{
    if (argc > 1) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "ls %s", argv[1]);
        run_system_cmd(cmd);
    } else {
        run_system_cmd("ls");
    }
}
#endif

#ifdef CONFIG_CLI_CMD_UPTIME
static void cmd_uptime(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    run_system_cmd("uptime");
}
#endif

#ifdef CONFIG_CLI_CMD_DATE
static void cmd_date(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    run_system_cmd("date");
}
#endif

#ifdef CONFIG_CLI_CMD_UNAME
static void cmd_uname(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    run_system_cmd("uname -a");
}
#endif

#ifdef CONFIG_CLI_CMD_PS
static void cmd_ps(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    run_system_cmd("ps aux");
}
#endif

#ifdef CONFIG_CLI_CMD_FREE
static void cmd_free(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    run_system_cmd("free -h");
}
#endif

int main(void)
{
    cli_cmd_t *start;
    cli_cmd_t *stop;

    cli_init(&cli, CONFIG_CLI_PROMPT);

    /* Top-level commands */
#ifdef CONFIG_CLI_CMD_HELP
    cli_register(&cli, NULL, "help", "Show available commands", cmd_help);
#endif
#ifdef CONFIG_CLI_CMD_VERSION
    cli_register(&cli, NULL, "version", "Show version", cmd_version);
#endif
#ifdef CONFIG_CLI_CMD_ECHO
    cli_register(&cli, NULL, "echo", "Echo arguments", cmd_echo);
#endif
#ifdef CONFIG_CLI_CMD_CLEAR
    cli_register(&cli, NULL, "clear", "Clear screen", cmd_clear);
#endif

    /* quit / exit */
    cli_register(&cli, NULL, "quit", "Quit the CLI", cmd_quit);
    cli_register(&cli, NULL, "exit", "Exit the CLI", cmd_quit);

    /* 2-level: start <timer|log>, stop <timer|log> */
    start = cli_register(&cli, NULL, "start", "Start a service", NULL);
    cli_register(&cli, start, "timer", "Start timer", cmd_start_timer);
    cli_register(&cli, start, "log", "Start logging", cmd_start_log);

    stop = cli_register(&cli, NULL, "stop", "Stop a service", NULL);
    cli_register(&cli, stop, "timer", "Stop timer", cmd_stop_timer);
    cli_register(&cli, stop, "log", "Stop logging", cmd_stop_log);

    /* 3-level: net if <up|down|status>, net dns <set|get> */
    cli_cmd_t *net = cli_register(&cli, NULL, "net", "Network commands", NULL);
    cli_cmd_t *net_if = cli_register(&cli, net, "if",
                                      "Interface control", NULL);
    cli_register(&cli, net_if, "up", "Bring interface up", cmd_net_if_up);
    cli_register(&cli, net_if, "down", "Bring interface down",
                 cmd_net_if_down);
    cli_register(&cli, net_if, "status", "Show interface status",
                 cmd_net_if_status);
    cli_cmd_t *net_dns = cli_register(&cli, net, "dns",
                                       "DNS configuration", NULL);
    cli_register(&cli, net_dns, "set", "Set DNS server", cmd_net_dns_set);
    cli_register(&cli, net_dns, "get", "Get DNS server", cmd_net_dns_get);

    /* 4-level: sys config wifi <ssid|password|save> */
    cli_cmd_t *sys = cli_register(&cli, NULL, "sys", "System management", NULL);
    cli_cmd_t *sys_cfg = cli_register(&cli, sys, "config",
                                       "Configuration", NULL);
    cli_cmd_t *sys_cfg_wifi = cli_register(&cli, sys_cfg, "wifi",
                                            "WiFi settings", NULL);
    cli_register(&cli, sys_cfg_wifi, "ssid", "Set WiFi SSID",
                 cmd_sys_cfg_wifi_ssid);
    cli_register(&cli, sys_cfg_wifi, "password", "Set WiFi password",
                 cmd_sys_cfg_wifi_pass);
    cli_register(&cli, sys_cfg_wifi, "save", "Save WiFi config",
                 cmd_sys_cfg_wifi_save);

    /* System info commands */
#ifdef CONFIG_CLI_CMD_LS
    cli_register(&cli, NULL, "ls", "List directory contents", cmd_ls);
#endif
#ifdef CONFIG_CLI_CMD_UPTIME
    cli_register(&cli, NULL, "uptime", "Show system uptime", cmd_uptime);
#endif
#ifdef CONFIG_CLI_CMD_DATE
    cli_register(&cli, NULL, "date", "Show current date/time", cmd_date);
#endif
#ifdef CONFIG_CLI_CMD_UNAME
    cli_register(&cli, NULL, "uname", "Show system information", cmd_uname);
#endif
#ifdef CONFIG_CLI_CMD_PS
    cli_register(&cli, NULL, "ps", "List running tasks", cmd_ps);
#endif
#ifdef CONFIG_CLI_CMD_FREE
    cli_register(&cli, NULL, "free", "Show memory usage", cmd_free);
#endif

    cli_port_puts("mini-cli v" CONFIG_CLI_VERSION
                  " — Tab to complete, Up/Down for history, "
                  "Ctrl-D to exit\r\n");

    cli_run(&cli);

    cli_port_puts("Bye.\r\n");
    return 0;
}
