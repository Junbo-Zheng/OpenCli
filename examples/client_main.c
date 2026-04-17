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
 * @file client_main.c
 * @brief CLI remote client application — connects to a running CLI server.
 *
 * Usage: cli_client [socket_path]
 */

#include "cli_client.h"
#include "cli_port.h"

int main(int argc, char *argv[])
{
    cli_client_t client;
    const char *path = argc > 1 ? argv[1] : NULL;

    cli_port_puts("Connecting to CLI server...\r\n");

    if (cli_client_init(&client, path, CONFIG_CLI_PROMPT) < 0) {
        cli_port_puts("Error: cannot connect to server at ");
        cli_port_puts(path ? path : CONFIG_CLI_NET_SOCKET_PATH);
        cli_port_puts("\r\n");
        return 1;
    }

    cli_port_puts("Connected. Type 'quit' or Ctrl-D to disconnect.\r\n");
    cli_client_run(&client);
    cli_client_deinit(&client);

    return 0;
}
