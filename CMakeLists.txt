# Copyright 2026 Junbo Zheng
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.27)
project(mini-cli LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Kconfig integration (parses .config into CONFIG_* CMake variables)
include(cmake/kconfig.cmake)

# CLI core static library
add_subdirectory(src)

# Example application (optional)
if(CONFIG_CLI_BUILD_EXAMPLES)
    add_subdirectory(examples)

    # Testing (linux only)
    if(NOT CONFIG_CLI_PLATFORM_RTOS)
        enable_testing()
        find_program(EXPECT_BIN expect)
        if(EXPECT_BIN)
            add_test(NAME cli_test
                     COMMAND ${EXPECT_BIN} ${CMAKE_SOURCE_DIR}/tests/test_cli.exp
                             $<TARGET_FILE:cli>)
        endif()
    endif()
endif()
