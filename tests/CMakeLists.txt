# SPDX-FileCopyrightText: 2025-2026 Igal Alkon
# SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.21)
project(bedrock_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Tests
add_executable(bedrock_tests
        test_main.c
        test_dynmap.c
        test_dynvec.c
        test_args.c
        test_arena.c
        test_parse.c
        test_vmem.c
        test_pinned_colony.c
        test_pmr.c
        test_fmap.c
        test_result.c
)

target_link_libraries(bedrock_tests PRIVATE bedrock_static)

# One call into each compiled module. Linking bedrock_shared fails the build
# when a Windows DLL exports none of those symbols.
add_executable(bedrock_shared_link
        link_shared.c
)

target_link_libraries(bedrock_shared_link PRIVATE bedrock_shared)

# Benchmarks
add_executable(bedrock_benchmarks
        bench_main.c
        bench_arena.c
        bench_pinned_colony.c
)

target_link_libraries(bedrock_benchmarks PRIVATE bedrock_static)
