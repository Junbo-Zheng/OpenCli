# OpenCli

[![CI](https://github.com/Junbo-Zheng/OpenCli/actions/workflows/ci.yml/badge.svg)](https://github.com/Junbo-Zheng/OpenCli/actions/workflows/ci.yml)
[![Lint](https://github.com/Junbo-Zheng/OpenCli/actions/workflows/lint.yml/badge.svg)](https://github.com/Junbo-Zheng/OpenCli/actions/workflows/lint.yml)
[![License: Apache-2.0](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE)
[![C11](https://img.shields.io/badge/C-11-00599C.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))

A portable, RTOS-friendly command-line framework in pure C — sub-command tree,
multi-level tab completion, arrow-key history, and optional client/server mode
over IPC.

Zero dynamic allocation. Zero external dependencies. A single ~80-line port
layer is all you need to bring it up on a new platform.

## Features

- **Hierarchical commands** — register arbitrarily deep sub-command trees
  (`net if status`, `sys config wifi ssid <name>`).
- **Multi-level tab completion** — complete command names at any depth,
  with automatic candidate listing for ambiguous prefixes.
- **Line editing & history** — arrow-key navigation, Ctrl-U/W/A/E, Ctrl-D
  to exit, configurable Ctrl-C behavior.
- **Static storage only** — a single `cli_t` instance, no `malloc`.
- **Kconfig-driven** — `menuconfig` for interactive tuning, `defconfig` for
  reproducible builds.
- **Client/server mode (optional)** — a lightweight wire protocol lets remote
  processes drive the CLI over Unix sockets or POSIX message queues.
- **Sanitizer-clean** — ASan + UBSan enabled by default on Linux.

## Getting started

### Requirements

- CMake ≥ 3.27 and Ninja (or any other CMake generator)
- A C11 compiler (GCC or Clang)
- Python 3 with [`kconfiglib`](https://pypi.org/project/kconfiglib/)
- `expect` (only needed to run the integration tests)

Install on Ubuntu/Debian:

```bash
sudo apt-get install -y cmake ninja-build expect
pip3 install kconfiglib
```

Install on macOS:

```bash
brew install cmake ninja expect
pip3 install kconfiglib
```

### Build & run

```bash
cmake -B build -G Ninja
cmake --build build
./build/examples/cli
```

You'll land in an interactive prompt:

```text
OpenCli v2026.04.17 — Tab to complete, Up/Down for history, Ctrl-D to exit
cli> hel<TAB>
cli> help
Available commands:
  help                        Show available commands
  version                     Show version
  echo                        Echo arguments
  start timer                 Start timer
  start log                   Start logging
  net if up                   Bring interface up
  net if status               Show interface status
  sys config wifi ssid        Set WiFi SSID
  ...
cli> net if <TAB>
up      down    status
cli>
```

### Run the tests

```bash
ctest --test-dir build --output-on-failure
```

## Configuration

OpenCli uses Kconfig. Three ways to tweak it:

```bash
# Interactive menu
cmake --build build -t menuconfig

# Regenerate cli_config.h after editing .config
cmake --build build -t genconfig

# Snapshot a minimal defconfig for version control
cmake --build build -t savedefconfig
```

Common options (see `src/Kconfig` and `examples/Kconfig` for the full list):

| Option                       | Default     | Meaning                              |
| ---------------------------- | ----------- | ------------------------------------ |
| `CONFIG_CLI_MAX_LINE`        | 256         | Input line buffer size (bytes)       |
| `CONFIG_CLI_MAX_HISTORY`     | 16          | History depth                        |
| `CONFIG_CLI_MAX_COMMANDS`    | 32          | Top-level command slots              |
| `CONFIG_CLI_MAX_SUB_COMMANDS`| 16          | Sub-commands per parent              |
| `CONFIG_CLI_CTRL_C_*`        | `QUIT`      | Ctrl-C behavior (quit/clear/ignore)  |
| `CONFIG_CLI_PLATFORM_LINUX`  | y           | Use the Linux/macOS termios port     |
| `CONFIG_CLI_PLATFORM_RTOS`   | n           | Use a user-provided UART port        |
| `CONFIG_CLI_NET_ENABLE`      | n           | Enable client/server mode            |
| `CONFIG_CLI_ASAN`            | y (Linux)   | Build with AddressSanitizer          |
| `CONFIG_CLI_UBSAN`           | y (Linux)   | Build with UndefinedBehaviorSanitizer|

## Registering commands

```c
#include "cli.h"

static cli_t cli;

static void cmd_hello(int argc, char *argv[])
{
    cli_port_puts("Hello from OpenCli!\r\n");
}

static void cmd_net_if_status(int argc, char *argv[])
{
    cli_port_puts("eth0: UP 192.168.1.100/24\r\n");
}

int main(void)
{
    cli_init(&cli, "cli> ");

    cli_register(&cli, NULL, "hello", "Say hello", cmd_hello);

    /* Sub-command tree: net if status / net if up / net if down */
    cli_cmd_t *net    = cli_register(&cli, NULL, "net",   "Network",           NULL);
    cli_cmd_t *net_if = cli_register(&cli, net,  "if",    "Interface control", NULL);
    cli_register(&cli, net_if, "status", "Show status", cmd_net_if_status);

    cli_run(&cli);
    return 0;
}
```

Each `cli_register` call returns a handle you can pass as the `parent`
argument to nest further. Trees of any depth are supported — tab completion
walks them automatically.

## Client/server mode

Enable `CONFIG_CLI_NET_ENABLE=y` to split the CLI into a **server** that
embeds in your main process and one or more **remote clients** that connect
over a Unix domain socket (or POSIX message queue).

```text
┌─────────────────────────────┐     ┌──────────────────────────────┐
│ cli_client (remote process) │     │ cli_server (main process)    │
│  Local line editing         │ ──► │  cli_t (commands registered) │
│  Local history              │ ◄── │  poll() multi-client loop    │
│  Tab/Cmd → protocol frames  │     │  Executes and returns output │
└─────────────────────────────┘     └──────────────────────────────┘
                 Unix socket (default) or POSIX mq
```

```bash
# Enable networking and rebuild
python3 -c "
import kconfiglib, os
os.environ['srctree'] = '.'
k = kconfiglib.Kconfig('Kconfig', suppress_traceback=True)
k.load_config('defconfig')
k.syms['CLI_NET_ENABLE'].set_value(2)
k.syms['CLI_NET_SERVER'].set_value(2)
k.syms['CLI_NET_CLIENT'].set_value(2)
k.write_config('.config')
"
cmake -B build -G Ninja && cmake --build build

# Terminal 1
./build/examples/cli_server

# Terminal 2 — full tab completion & history across the wire
./build/examples/cli_client
```

The protocol is a 3-byte header (`type` + big-endian `length`) followed by
an opaque payload. See [`src/cli_protocol.h`](src/cli_protocol.h) for the
full frame definition.

## Porting to your platform

The CLI core touches hardware exclusively through
[`src/cli_port.h`](src/cli_port.h). To bring up a new target, implement six
functions:

```c
int  cli_port_getchar(void);          /* blocking, one byte or -1 on EOF */
void cli_port_putchar(char c);
void cli_port_puts(const char *s);
void cli_port_flush(void);
void cli_port_raw_mode(void);         /* no-op on raw UART */
void cli_port_restore_mode(void);
```

[`src/cli_port_linux.c`](src/cli_port_linux.c) is a complete termios-based
reference. [`src/cli_port_rtos.c`](src/cli_port_rtos.c) is a ready-to-fill
stub for RTOS integration — wire it to your UART driver and set
`CONFIG_CLI_PLATFORM_RTOS=y`.

> [!TIP]
> Because the core uses only static storage and the six port calls, it
> drops into FreeRTOS, Zephyr, NuttX, or bare-metal with no further changes.

## Project layout

```text
.
├── src/               # CLI core library
│   ├── cli.{c,h}      # Line editing, completion, history
│   ├── cli_port*.{c,h}# Platform port layer
│   ├── cli_server.*   # Embedded server (optional)
│   ├── cli_client.*   # Remote client (optional)
│   ├── cli_transport_*# Unix socket / POSIX mq transports
│   └── cli_protocol.h # Wire protocol frames
├── examples/          # Demo application with rich sub-command tree
├── tests/             # expect-based integration tests
├── cmake/             # Kconfig ↔ CMake glue
├── Kconfig            # Top-level configuration
└── defconfig          # Minimal default configuration
```

> [!NOTE]
> `defconfig` is the source of truth. `.config` is generated from it on the
> first build and regenerated by `menuconfig`; do not commit it.
