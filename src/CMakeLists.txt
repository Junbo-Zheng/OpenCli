# Copyright 2026 Junbo Zheng
# SPDX-License-Identifier: Apache-2.0

# CLI core static library

add_library(cli_lib STATIC
    cli.c
    ${CLI_PORT_SRC_FILE}
)

target_include_directories(cli_lib
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC ${CMAKE_BINARY_DIR}
)

target_compile_options(cli_lib PUBLIC -Wall -Wextra -Werror)

# Sanitizers
set(SANITIZER_FLAGS "")
if(CONFIG_CLI_ASAN)
    list(APPEND SANITIZER_FLAGS -fsanitize=address -fno-omit-frame-pointer)
endif()
if(CONFIG_CLI_UBSAN)
    list(APPEND SANITIZER_FLAGS -fsanitize=undefined)
endif()
if(SANITIZER_FLAGS)
    target_compile_options(cli_lib PUBLIC ${SANITIZER_FLAGS})
    target_link_options(cli_lib PUBLIC ${SANITIZER_FLAGS})
endif()

set_target_properties(cli_lib PROPERTIES OUTPUT_NAME cli)
