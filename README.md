# mini-cli

Portable CLI framework with sub-command tree, tab completion, and command history. Pure C, zero dynamic allocation, zero external dependencies — designed for RTOS porting.

Supports **client/server mode**: server embeds into the main process, remote clients connect with full tab completion and history.

## Architecture

### Standalone Mode (default)

```
┌──────────────────────────────────────────────┐
│           examples/main.c                    │  Application: register commands
├──────────────────────────────────────────────┤
│           src/cli.h + cli.c                  │  Core: line editing, completion, history
├──────────────────────────────────────────────┤
│             src/cli_port.h                   │  Port interface (6 functions)
├───────────────────┬──────────────────────────┤
│ cli_port_linux.c  │  cli_port_rtos.c         │  Platform implementations
│ (Linux / macOS)   │  (you implement)         │
└───────────────────┴──────────────────────────┘
```

### Client/Server Mode (`CONFIG_CLI_NET_ENABLE=y`)

```
┌─────────────────────────────┐     ┌──────────────────────────────┐
│ cli_client (remote process) │     │ cli_server (main process)    │
│  Local line editing         │     │  cli_t (commands registered) │
│  Local history              │     │  poll() multi-client loop    │
│  Tab → TAB request ─────────┼────►│  Execute CMD / resolve TAB   │
│  Enter → CMD request ───────┼────►│  Return RSP / COMP ──────────┼──►
│  Display response ◄─────────┼─────┤  Output capture hook         │
├─────────────────────────────┤     ├──────────────────────────────┤
│     cli_transport.h         │     │     cli_transport.h          │
├──────────┬──────────────────┤     ├──────────┬───────────────────┤
│ unix.c   │  mq.c (RTOS)     │     │ unix.c   │  mq.c (RTOS)      │
└──────────┴──────────────────┘     └──────────┴───────────────────┘
```

## Features

- **Sub-command tree** — multi-level commands (`net if up`, `sys config wifi ssid`), unlimited depth
- **Tab completion** — works at every level, single match auto-completes, multiple shows candidates
- **Command history** — Up/Down arrow, configurable depth, duplicate suppression
- **Line editing** — Left/Right cursor, Backspace, Delete, Home/End
- **Shortcuts** — Ctrl-A/E/K/U/L/D, configurable Ctrl-C (quit/clear-line/ignore)
- **Client/server** — remote CLI access with full tab completion and history over Unix socket or POSIX mq
- **Multi-client** — server supports up to N concurrent clients (configurable)
- **Pure C11** — no C++, no STL, no malloc, no external libraries
- **Port layer** — implement 6 functions to run on any platform
- **Transport port** — implement 1 file to add new IPC mechanisms
- **Kconfig** — menuconfig for build-time configuration (NuttX style)
- **Auto-reconfigure** — `.config` changes auto-trigger CMake reconfigure on next build
- **CI** — GitHub Actions with gcc/clang on Linux + clang on macOS

## Quick Start

### Standalone Mode

```bash
cmake -B build -G Ninja
cmake --build build -j
./build/examples/cli
```

### Client/Server Mode

```bash
# Enable networking via menuconfig
cmake --build build -t menuconfig
# → CLI Networking → Enable client/server mode → y

# Build
cmake --build build -j

# Terminal 1: start server
./build/examples/cli_server

# Terminal 2: connect client
./build/examples/cli_client
```

## Configuration

```bash
# Interactive menuconfig
cmake --build build -t menuconfig

# Build (auto-reconfigures if .config changed)
cmake --build build -j

# Save minimal sorted defconfig (NuttX style)
cmake --build build -t savedefconfig
```

### Kconfig Options

**CLI Core:**

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `CONFIG_CLI_VERSION` | string | `"YYYY.MM.DD"` | Version string |
| `CONFIG_CLI_PROMPT` | string | `"cli> "` | Default prompt string |
| `CONFIG_CLI_MAX_LINE` | int | 256 | Max input line length |
| `CONFIG_CLI_MAX_HISTORY` | int | 16 | History buffer depth |
| `CONFIG_CLI_MAX_COMMANDS` | int | 32 | Max registered commands |
| `CONFIG_CLI_MAX_SUB_COMMANDS` | int | 16 | Max sub-commands per command |
| `CONFIG_CLI_CTRL_C_*` | choice | Quit | Ctrl-C behavior |

**CLI Networking:**

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `CONFIG_CLI_NET_ENABLE` | bool | n | Enable client/server mode |
| `CONFIG_CLI_NET_TRANSPORT_UNIX` | choice | y | Unix domain socket transport |
| `CONFIG_CLI_NET_TRANSPORT_MQ` | choice | n | POSIX message queue transport |
| `CONFIG_CLI_NET_SERVER` | bool | y | Build server library |
| `CONFIG_CLI_NET_CLIENT` | bool | y | Build client application |
| `CONFIG_CLI_NET_MAX_CLIENTS` | int | 4 | Max concurrent clients |
| `CONFIG_CLI_NET_SOCKET_PATH` | string | `/tmp/cli.sock` | Socket/endpoint path |

**Built-in Commands** (each independently toggleable):

`help`, `version`, `echo`, `clear`

**Example Commands** (Linux/macOS: calls real system commands via `popen`):

`ls`, `uptime`, `date`, `uname`, `ps`, `free`

**Platform:**

| Option | Description |
|--------|-------------|
| `CONFIG_CLI_PLATFORM_LINUX` | Linux/macOS (termios) |
| `CONFIG_CLI_PLATFORM_RTOS` | RTOS (user-provided UART) |

**Debug:**

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `CONFIG_CLI_ASAN` | bool | y (Linux) | AddressSanitizer |
| `CONFIG_CLI_UBSAN` | bool | y (Linux) | UndefinedBehaviorSanitizer |

> Note: Sanitizers are automatically disabled on macOS regardless of Kconfig
> settings. macOS clang's ASan/UBSan have compatibility issues with termios
> raw mode and `popen()`, causing illegal hardware instruction crashes.

## Wire Protocol

Client/server communication uses a binary frame protocol:

```
[1 byte type][2 bytes payload length (big-endian)][payload]

Types:
  0x01 CMD  — Client → Server: execute command line
  0x02 TAB  — Client → Server: request tab completion
  0x03 RSP  — Server → Client: command output text
  0x04 COMP — Server → Client: tab candidates (\t separated)
```

## API

### Standalone

```c
cli_t cli;
cli_init(&cli, "mydev> ");
cli_register(&cli, NULL, "reboot", "Reboot system", cmd_reboot);
cli_run(&cli);
```

### Server (embed in main process)

```c
cli_t cli;
cli_server_t server;

cli_init(&cli, CONFIG_CLI_PROMPT);
cli_register(&cli, NULL, "reboot", "Reboot", cmd_reboot);

cli_server_install_signal_handler(&server);  /* graceful SIGTERM/SIGINT */
cli_server_init(&server, &cli, NULL);        /* NULL = default socket path */
cli_server_run(&server);                     /* blocking poll loop */
cli_server_deinit(&server);
```

> `cli_server_install_signal_handler()` catches SIGTERM/SIGINT and sets
> `server.running = 0` so the poll loop exits cleanly. Without it, killing
> the server while blocked in `poll()` causes ASan to report
> `DEADLYSIGNAL` — ASan intercepts the fatal signal but cannot unwind the
> stack from inside a syscall, resulting in an infinite error loop.

### Client (separate process)

```c
cli_client_t client;
cli_client_init(&client, NULL, "mydev> ");  /* NULL = default socket path */
cli_client_run(&client);                     /* blocking interactive loop */
cli_client_deinit(&client);
```

## Testing

Tests use [expect](https://core.tcl-lang.org/expect) to simulate interactive terminal input.

```bash
# Standalone mode tests (via CTest)
ctest --test-dir build --output-on-failure

# Client/server mode tests (manual)
./build/examples/cli_server &
expect tests/test_client.exp ./build/examples/cli_client
kill %1
```

> Testing is only enabled for standalone Linux builds. Server mode and RTOS builds skip CTest.

## CI

GitHub Actions runs on every push/PR to master:

| OS | Compiler | Mode | Tests |
|----|----------|------|-------|
| Ubuntu | gcc | standalone | ✅ |
| Ubuntu | clang | standalone | ✅ |
| macOS | clang | standalone | ✅ |
| Ubuntu | gcc | client/server | ✅ |

All builds use `-Wall -Wextra -Werror`.

## RTOS Porting

Implement 6 functions in `cli_port.h`:

```c
int  cli_port_getchar(void);
void cli_port_putchar(char c);
void cli_port_puts(const char *s);
void cli_port_flush(void);
void cli_port_raw_mode(void);
void cli_port_restore_mode(void);
```

For client/server on RTOS, implement `cli_transport_mq.c` (POSIX mq) or your own transport.

## Project Structure

```
├── CMakeLists.txt
├── cmake/
│   └── kconfig.cmake               # Kconfig integration + auto-reconfigure
├── Kconfig
├── defconfig
├── genconfig.py
├── src/
│   ├── CMakeLists.txt              # Builds libcli.a
│   ├── Kconfig                     # Core, networking, platform, debug config
│   ├── cli.h / cli.c               # Core: line editing, completion, history
│   ├── cli_port.h                  # Port layer interface
│   ├── cli_port_linux.c            # Linux/macOS port (+ output hook)
│   ├── cli_port_rtos.c             # RTOS port template
│   ├── cli_protocol.h              # Wire protocol (CMD/TAB/RSP/COMP)
│   ├── cli_transport.h             # Transport port interface
│   ├── cli_transport_unix.c        # Unix domain socket transport
│   ├── cli_transport_mq.c          # POSIX mq transport template
│   ├── cli_server.h / cli_server.c # Server: multi-client poll loop
│   └── cli_client.h / cli_client.c # Client: local editing + remote exec
├── examples/
│   ├── CMakeLists.txt
│   ├── Kconfig
│   ├── main.c                       # Server or standalone demo
│   └── client_main.c                # Client demo
├── tests/
│   ├── test_cli.exp                 # Standalone interactive test
│   └── test_client.exp              # Client/server test
├── .github/workflows/ci.yml
└── LICENSE                          # Apache 2.0
```

## License

Apache License 2.0 — see [LICENSE](LICENSE) for details.
