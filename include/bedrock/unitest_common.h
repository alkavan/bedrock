// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file
 * @brief Assertions, tolerances, token pasting, and startup registration.
 *
 * `REQUIRE` and `FAIL` print a failure and throw. `INITIALIZER` runs a
 * function before `main`. `CONCAT` pastes two tokens. `UNITEST_EPS_32`,
 * `UNITEST_EPS_64`, `EPSILON`, and `EPSILON64` are the comparison tolerances.
 */

#ifndef BEDROCK_UNITEST_COMMON_H
#define BEDROCK_UNITEST_COMMON_H

#include <stdio.h>

#include "except.h"

// Epsilon
#define UNITEST_EPS_32 1e-6f
#define UNITEST_EPS_64 1e-12

extern const float EPSILON;
extern const double EPSILON64;

// Token pasting helpers
#define CONCAT_(x, y) x ## y
#define CONCAT(x, y) CONCAT_(x, y)

// Portable initializer macro
#ifdef _MSC_VER
#pragma section(".CRT$XCU", read)
#ifdef _WIN64
#define INITIALIZER(f) INITIALIZER_I(f, "")
#else
#define INITIALIZER(f) INITIALIZER_I(f, "_")
#endif
#define INITIALIZER_I(f, p) \
    static void f(void); \
    __declspec(allocate(".CRT$XCU")) void (*f##_)(void) = f; \
    __pragma(comment(linker,"/include:" p #f "_")) \
    static void f(void)
#else
#define INITIALIZER(f)                                \
    static void f(void) __attribute__((constructor)); \
    static void f(void)
#endif

/*******************************************************************************
 * Assertion Macros
 ******************************************************************************/

/**
 * @brief Fail the current test when `cond` is false.
 *
 * Prints the condition and source location, then `THROW`.
 *
 * @param cond Expression to test.
 */
#define REQUIRE(cond) do {                                                  \
    if (!(cond)) {                                                          \
        printf("REQUIRE failed: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
        THROW;                                                              \
    }                                                                       \
} while (0)

/**
 * @brief Fail the current test with a printf message, then `THROW`.
 *
 * @param fmt printf format string.
 * @param ... Arguments for `fmt`.
 */
#define FAIL(fmt, ...) do {                                                \
    printf("FAIL: " fmt " at %s:%d\n", ##__VA_ARGS__, __FILE__, __LINE__); \
    THROW;                                                                 \
} while (0)

#endif /* BEDROCK_UNITEST_COMMON_H */
