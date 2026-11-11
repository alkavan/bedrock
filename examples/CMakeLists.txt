# SPDX-FileCopyrightText: 2025-2026 Igal Alkon
# SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.21)
project(bedrock_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

add_executable(pinned_colony_example
        pinned_colony_example.c
)

target_link_libraries(pinned_colony_example PRIVATE bedrock_static)

add_executable(args_example
        args_example.c
)

target_link_libraries(args_example PRIVATE bedrock_static)

add_executable(containers_example
        containers_example.c
)

target_link_libraries(containers_example PRIVATE bedrock_static)
