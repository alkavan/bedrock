// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#include <bedrock/unitest.h>
#include <bedrock.h>

// Epsilon
const float EPSILON = UNITEST_EPS_32;
const double EPSILON64 = UNITEST_EPS_64;

// Test node
struct test_node* test_head = NULL;

// Test exception buffer
jmp_buf _except_buf;

// Benchmark node
struct benchmark_node* benchmark_head = NULL;

int main(int argc, char *argv[])
{
    printf("Bedrock %s — A bedrock utility library in C.\n", BEDROCK_VERSION);
    return run_tests();
}
