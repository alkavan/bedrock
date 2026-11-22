// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#include <bedrock.h>
#include <bedrock/unitest.h>

// Epsilon
const float EPSILON = UNITEST_EPS_32;
const double EPSILON64 = UNITEST_EPS_64;

// Test exception buffer
jmp_buf _except_buf;

// Test node
struct TestNode* test_head = NULL;

int main(int argc, char *argv[])
{
    printf("Bedrock %s — A bedrock utility library in C.\n", BEDROCK_VERSION);
    return run_tests();
}
