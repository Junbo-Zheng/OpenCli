# Copyright 2026 Junbo Zheng
# SPDX-License-Identifier: Apache-2.0

add_executable(cli main.c)
target_link_libraries(cli PRIVATE cli_lib)
