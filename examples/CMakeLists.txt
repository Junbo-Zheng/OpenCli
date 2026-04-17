# Copyright 2026 Junbo Zheng
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_CLI_NET_ENABLE AND CONFIG_CLI_NET_SERVER)
    add_executable(cli_server main.c)
    target_link_libraries(cli_server PRIVATE cli_lib)
else()
    add_executable(cli main.c)
    target_link_libraries(cli PRIVATE cli_lib)
endif()

# Remote client application
if(CONFIG_CLI_NET_ENABLE AND CONFIG_CLI_NET_CLIENT)
    add_executable(cli_client client_main.c)
    target_link_libraries(cli_client PRIVATE cli_lib)
endif()
