// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#ifndef BEDROCK_CONFIG_H
#define BEDROCK_CONFIG_H

/**
 * @file
 * @brief Shared-library export and deprecation macros.
 */

/**
 * @brief Marks a function defined in libbedrock.
 *
 * `__declspec(dllexport)` while the shared library is compiled on Windows
 * (`BEDROCK_BUILD_SHARED`). Empty for the static library, for callers, and
 * on other systems. There is no `dllimport`: the library exports functions,
 * and one empty macro stays valid for both library types.
 *
 * Write `BED_API` before `BED_DEPRECATED` when a declaration uses both.
 */
#if defined(_WIN32) && defined(BEDROCK_BUILD_SHARED)
#  define BED_API __declspec(dllexport)
#else
#  define BED_API
#endif

/**
 * @brief Marks a public function as deprecated.
 *
 * `msg` names the replacement, for example `"use arena_reset"`. Empty when the
 * compiler has no deprecation attribute.
 */
#if defined(_MSC_VER)
#  define BED_DEPRECATED(msg) __declspec(deprecated(msg))
#elif defined(__GNUC__) || defined(__clang__)
#  define BED_DEPRECATED(msg) __attribute__((deprecated(msg)))
#else
#  define BED_DEPRECATED(msg)
#endif

#endif /* BEDROCK_CONFIG_H */
