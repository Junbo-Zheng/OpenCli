# mini-cli

Portable CLI framework with sub-command tree, tab completion, and command history. Pure C, zero dynamic allocation, zero external dependencies — designed for RTOS porting.

## Architecture

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

## Features

- **Sub-command tree** — multi-level commands (`net if up`, `sys config wifi ssid`), unlimited depth
- **Tab completion** — works at every level, single match auto-completes, multiple shows candidates
- **Command history** — Up/Down arrow, configurable depth, duplicate suppression
- **Line editing** — Left/Right cursor, Backspace, Delete, Home/End
- **Shortcuts** — Ctrl-A/E/K/U/L/D, configurable Ctrl-C (quit/clear-line/ignore)
- **Pure C11** — no C++, no STL, no malloc, no external libraries
- **Port layer** — implement 6 functions to run on any platform
- **Kconfig** — menuconfig for build-time configuration (NuttX style)
- **CI** — GitHub Actions with gcc/clang on Linux + clang on macOS

## Quick Start

```bash
# Build (default: Linux, all commands enabled)
cmake -B build -G Ninja
cmake --build build -j

# Run
./build/examples/cli
```

## Configuration

```bash
# Interactive menuconfig
cmake --build build -t menuconfig

# Regenerate header after config change
cmake --build build -t genconfig
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
| `CONFIG_CLI_CTRL_C_QUIT` | choice | y | Ctrl-C quits CLI |
| `CONFIG_CLI_CTRL_C_CLEAR_LINE` | choice | n | Ctrl-C clears current line |
| `CONFIG_CLI_CTRL_C_IGNORE` | choice | n | Ctrl-C ignored |

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
| `CONFIG_CLI_ASAN` | bool | y (Linux) | AddressSanitizer — detect memory errors |
| `CONFIG_CLI_UBSAN` | bool | y (Linux) | UndefinedBehaviorSanitizer — detect UB |

**Build:**

| Option | Default | Description |
|--------|---------|-------------|
| `CONFIG_CLI_BUILD_EXAMPLES` | y | Build example application (set n for library only) |

## API

Single unified registration API:

```c
/* Initialize */
cli_t cli;
cli_init(&cli, "mydev> ");

/* Top-level command (parent = NULL) */
cli_register(&cli, NULL, "reboot", "Reboot system", cmd_reboot);

/* Sub-command tree (parent = node pointer) */
cli_cmd_t *net = cli_register(&cli, NULL, "net", "Network", NULL);
cli_cmd_t *net_if = cli_register(&cli, net, "if", "Interface", NULL);
cli_register(&cli, net_if, "up", "Bring up", cmd_net_if_up);
cli_register(&cli, net_if, "down", "Bring down", cmd_net_if_down);

/* Quit command */
cli_register(&cli, NULL, "quit", "Quit", my_quit_handler);

/* In quit handler: */
void my_quit_handler(int argc, char *argv[]) {
    cli_quit(&cli);
}

/* Run (blocks until quit or Ctrl-D) */
cli_run(&cli);
```

## Testing

Tests use [expect](https://core.tcl-lang.org/expect) to simulate interactive terminal input (Tab, arrow keys), which cannot be tested via pipe or shell scripts.

Prerequisites:

```bash
sudo apt install expect    # Linux
brew install expect        # macOS
```

Run tests:

```bash
# Method 1: via CTest (recommended)
# Manages test results, timeout, and output. Also used in CI.
ctest --test-dir build --output-on-failure

# Method 2: run expect script directly
# Useful for debugging — shows the full interactive process.
expect tests/test_cli.exp build/examples/cli
```

Test coverage: command execution, tab completion (single/multiple match), history (Up/Down), line editing (Ctrl-U), quit/exit.

> Note: Testing is only enabled for Linux/macOS platform (`CONFIG_CLI_PLATFORM_LINUX`). RTOS builds skip tests since the firmware cannot run directly on the host.

## CI

GitHub Actions runs on every push/PR to master:

| OS | Compiler | Status |
|----|----------|--------|
| Ubuntu | gcc | Build + Test |
| Ubuntu | clang | Build + Test |
| macOS | clang | Build + Test |

All builds use `-Wall -Wextra -Werror`.

## RTOS Porting

Implement 6 functions in `cli_port.h`:

```c
int  cli_port_getchar(void);       /* blocking read 1 byte */
void cli_port_putchar(char c);     /* write 1 byte */
void cli_port_puts(const char *s); /* write string */
void cli_port_flush(void);         /* flush output (can be no-op) */
void cli_port_raw_mode(void);      /* enter raw mode (can be no-op) */
void cli_port_restore_mode(void);  /* restore mode (can be no-op) */
```

Select RTOS platform via menuconfig or `.config`:

```
CONFIG_CLI_PLATFORM_RTOS=y
```

## Project Structure

```
├── CMakeLists.txt              # Top-level build
├── cmake/
│   └── kconfig.cmake           # Kconfig integration
├── Kconfig                     # Top-level config (sources src/ and examples/)
├── defconfig                   # Default configuration (minimal, sorted)
├── genconfig.py                # Generate cli_config.h / savedefconfig
├── src/
│   ├── CMakeLists.txt          # Builds libcli.a static library
│   ├── Kconfig                 # Core params, commands, platform config
│   ├── cli.h                   # Core API
│   ├── cli.c                   # Core implementation
│   ├── cli_port.h              # Port layer interface
│   ├── cli_port_linux.c        # Linux/macOS reference port
│   └── cli_port_rtos.c         # RTOS port template
├── examples/
│   ├── CMakeLists.txt          # Builds demo application
│   ├── Kconfig                 # CLI_BUILD_EXAMPLES + example command toggles
│   └── main.c                  # Demo: sub-commands, system commands
├── tests/
│   └── test_cli.exp            # Automated expect test
├── .github/
│   └── workflows/
│       └── ci.yml              # GitHub Actions CI (gcc/clang, Linux/macOS)
└── LICENSE                     # Apache 2.0
```

## License

Apache License 2.0 — see [LICENSE](LICENSE) for details.
