// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#include <bedrock.h>
#include <bedrock/unibench.h>
#include <stdlib.h>

// Epsilon
const float EPSILON = UNITEST_EPS_32;
const double EPSILON64 = UNITEST_EPS_64;

// Benchmark exception buffer
jmp_buf _except_buf;

// Benchmark node
struct benchmark_node* benchmark_head = NULL;

int main(const int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("Bedrock %s — running benchmark suite...\n", BEDROCK_VERSION);

    const double total_time = run_benchmarks();
    printf("All benchmarks completed in %.9lf seconds \n", total_time);

    return EXIT_SUCCESS;
}
