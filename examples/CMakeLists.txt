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

add_executable(pmr_example
        pmr_example.c
)

target_link_libraries(pmr_example PRIVATE bedrock_static)

add_executable(game_example
        game_example.c
)

target_link_libraries(game_example PRIVATE bedrock_static)

add_executable(result_example
        result_example.c
)

target_link_libraries(result_example PRIVATE bedrock_static)

# result.h is C11. Keep extensions off for this example.
set_target_properties(result_example PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(result_example PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
