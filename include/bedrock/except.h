// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file
 * @brief setjmp-based TRY and THROW macros.
 *
 * Tests define `jmp_buf _except_buf` once and use these macros to leave a
 * failed check. There is no stack of handlers: one buffer, one TRY.
 */

#ifndef BEDROCK_EXCEPT_H
#define BEDROCK_EXCEPT_H

#include <setjmp.h>

/** @brief Jump buffer for TRY and THROW. Define it once, in global scope. */
extern jmp_buf _except_buf;

/** @brief Start a try block. The matching CATCH runs when THROW is reached. */
#define TRY do { \
    if (setjmp(_except_buf) == 0) {

/** @brief Handle a THROW from the matching TRY. */
#define CATCH } else {

/** @brief Close a TRY and CATCH pair. */
#define END_TRY } } while (0)

/** @brief Jump to the matching CATCH. */
#define THROW longjmp(_except_buf, 1)

#endif /* BEDROCK_EXCEPT_H */
