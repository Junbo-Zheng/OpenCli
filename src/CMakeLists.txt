# Copyright 2026 Junbo Zheng
# SPDX-License-Identifier: Apache-2.0

# CLI core static library

set(CLI_LIB_SRCS cli.c ${CLI_PORT_SRC_FILE})

# Networking: transport + server + client
if(CONFIG_CLI_NET_ENABLE)
    if(CONFIG_CLI_NET_TRANSPORT_UNIX)
        list(APPEND CLI_LIB_SRCS cli_transport_unix.c)
    elseif(CONFIG_CLI_NET_TRANSPORT_MQ)
        list(APPEND CLI_LIB_SRCS cli_transport_mq.c)
    endif()

    if(CONFIG_CLI_NET_SERVER)
        list(APPEND CLI_LIB_SRCS cli_server.c)
    endif()

    if(CONFIG_CLI_NET_CLIENT)
        list(APPEND CLI_LIB_SRCS cli_client.c)
    endif()
endif()

add_library(cli_lib STATIC ${CLI_LIB_SRCS})

target_include_directories(cli_lib
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC ${CMAKE_BINARY_DIR}
)

target_compile_options(cli_lib PUBLIC -Wall -Wextra -Werror)

# Sanitizers (Linux only — macOS clang ASan/UBSan have compatibility issues
# with termios raw mode and popen, causing illegal instruction or hangs)
set(SANITIZER_FLAGS "")
if(NOT APPLE)
    if(CONFIG_CLI_ASAN)
        list(APPEND SANITIZER_FLAGS -fsanitize=address -fno-omit-frame-pointer)
    endif()
    if(CONFIG_CLI_UBSAN)
        list(APPEND SANITIZER_FLAGS -fsanitize=undefined)
    endif()
endif()
if(SANITIZER_FLAGS)
    target_compile_options(cli_lib PUBLIC ${SANITIZER_FLAGS})
    target_link_options(cli_lib PUBLIC ${SANITIZER_FLAGS})
endif()

set_target_properties(cli_lib PROPERTIES OUTPUT_NAME cli)
