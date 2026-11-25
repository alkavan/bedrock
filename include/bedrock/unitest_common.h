// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file
 * @brief Assertions, tolerances, token pasting, and startup registration.
 *
 * `REQUIRE` and `FAIL` print a failure and leave the current test.
 * `INITIALIZER` runs a function before `main`. `CONCAT` pastes two tokens.
 * `UNITEST_EPS_32`, `UNITEST_EPS_64`, `EPSILON`, and `EPSILON64` are the
 * comparison tolerances.
 */

#ifndef BEDROCK_UNITEST_COMMON_H
#define BEDROCK_UNITEST_COMMON_H

#include <setjmp.h>
#include <stdio.h>

/**
 * @brief Jump buffer a failed test check uses to leave the test.
 *
 * The test program defines this once, in global scope. One buffer, one
 * active check.
 */
extern jmp_buf _except_buf;

/** @brief Open the test runner's check region. */
#define TRY do { \
    if (setjmp(_except_buf) == 0) {

/** @brief Run when a check in the matching TRY fails. */
#define CATCH } else {

/** @brief Close a TRY and CATCH pair. */
#define END_TRY } } while (0)

/** @brief Leave the current test check. */
#define THROW longjmp(_except_buf, 1)

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
 * Prints the condition and source location, then leaves the test.
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
 * @brief Fail the current test with a printf message and leave the test.
 *
 * @param fmt printf format string.
 * @param ... Arguments for `fmt`.
 */
#define FAIL(fmt, ...) do {                                                \
    printf("FAIL: " fmt " at %s:%d\n", ##__VA_ARGS__, __FILE__, __LINE__); \
    THROW;                                                                 \
} while (0)

#endif /* BEDROCK_UNITEST_COMMON_H */
