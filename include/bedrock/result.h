// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file
 * @brief A value and an error returned together.
 *
 * A result carries a value and a `ResultError`. `ok` selects which one the
 * caller reads. The caller propagates that object. Success-path macros expand
 * in place.
 */

#ifndef BEDROCK_RESULT_H
#define BEDROCK_RESULT_H

#include "bedrock/config.h"

#include <stdbool.h>

/**
 * @brief Failure attached to a result.
 *
 * `code` is 0 when the operation succeeded. A failure uses a non-zero code.
 * `name`, `message`, and `file` are not copied; they must outlive the error.
 * String literals and `__FILE__` do.
 */
typedef struct ResultError {
    int code;            /**< 0 on success; a non-zero failure code otherwise. */
    const char *name;    /**< Short stable name, or NULL. Not copied. */
    const char *message; /**< Human-readable detail, or NULL. Not copied. */
    const char *file;    /**< Source file that created the error, or NULL. Not copied. */
    int line;            /**< Source line that created the error. 0 if unknown. */
} ResultError;

/**
 * @brief Build an error value.
 *
 * The string pointers are stored as given and are not copied.
 *
 * @param code Failure code. Zero means success; a failure uses a non-zero code.
 * @param name Short stable name. May be NULL.
 * @param message Human-readable detail. May be NULL.
 * @param file Source file. May be NULL.
 * @param line Source line.
 *
 * @return The error value. No allocation.
 */
static inline ResultError result_error(const int code, const char *name, const char *message,
                                        const char *file, const int line) {
    ResultError error;

    error.code = code;
    error.name = name;
    error.message = message;
    error.file = file;
    error.line = line;
    return error;
}

/**
 * @brief Print an error and abort the process.
 *
 * `BD_RESULT_UNWRAP` and `BD_RESULT_EXPECT` call this when `ok` is false.
 * A NULL `error` is reported as an unknown failure. A NULL or empty
 * `expectation` omits the extra note.
 *
 * @param error Error to report. May be NULL.
 * @param expectation Extra note, usually from `BD_RESULT_EXPECT`. May be NULL.
 * @param file Call site file. May be NULL.
 * @param line Call site line.
 */
BED_API _Noreturn void result_abort(const ResultError *error, const char *expectation,
                                    const char *file, int line);

/**
 * @def BD_RESULT_ERROR(code, name, message)
 * @brief Build an error stamped with the call site.
 *
 * @param code Failure code.
 * @param name Short stable name. May be NULL.
 * @param message Human-readable detail. May be NULL.
 */
#define BD_RESULT_ERROR(code, name, message) \
    result_error((code), (name), (message), __FILE__, __LINE__)

/**
 * @def BD_DEFINE_RESULT(Name, T)
 * @brief Define a result type that holds a `T` and a `ResultError`.
 *
 * `ok` is true on success and `value` holds the `T`. `error` is zeroed on
 * success. On failure `ok` is false, `error` holds the failure, and `value`
 * is zeroed. `T` is copied by assignment and must not contain a comma;
 * typedef such a type first. Define each name once in a translation unit.
 * The macro includes the typedef's semicolon.
 *
 * @param Name Result type name.
 * @param T Value type stored on success.
 */
#define BD_DEFINE_RESULT(Name, T) \
    typedef struct Name {         \
        bool ok;                  \
        T value;                  \
        ResultError error;        \
    } Name;

/* Parameter names stay off the member names. `.value` is the tokens `.` and
 * `value`, so a parameter named `value` would be rewritten inside the initializer. */

/**
 * @def BD_RESULT_OK(Type, ok_value)
 * @brief Success result of type `Type`.
 *
 * @param Type Result type from `BD_DEFINE_RESULT`.
 * @param ok_value Value to store.
 */
#define BD_RESULT_OK(Type, ok_value) \
    ((Type){ .ok = true, .value = (ok_value) })

/**
 * @def BD_RESULT_ERR(Type, failure)
 * @brief Failure result of type `Type`.
 *
 * @param Type Result type from `BD_DEFINE_RESULT`.
 * @param failure `ResultError` to store.
 */
#define BD_RESULT_ERR(Type, failure) \
    ((Type){ .ok = false, .error = (failure) })

/**
 * @def BD_RESULT_IS_OK(result)
 * @brief True when `result.ok` is true.
 *
 * @param result Result object. Pass a variable.
 */
#define BD_RESULT_IS_OK(result) ((result).ok)

/**
 * @def BD_RESULT_IS_ERR(result)
 * @brief True when `result.ok` is false.
 *
 * @param result Result object. Pass a variable.
 */
#define BD_RESULT_IS_ERR(result) (!(result).ok)

/**
 * @def BD_RESULT_TRY(Type, out, expr)
 * @brief Assign a successful value, or return the error from this function.
 *
 * Evaluates `expr` once. The enclosing function returns `Type`, and `expr`
 * has that same type. On failure `out` is left unchanged and the function
 * returns the error. On success `out` receives the value.
 *
 * @param Type Result type from `BD_DEFINE_RESULT`.
 * @param out Destination for the success value.
 * @param expr Expression of type `Type`.
 */
#define BD_RESULT_TRY(Type, out, expr)          \
    do {                                        \
        const Type bd_result_try_ = (expr);     \
        if (!bd_result_try_.ok) {               \
            return bd_result_try_;              \
        }                                       \
        (out) = bd_result_try_.value;           \
    } while (0)

/**
 * @def BD_RESULT_EXPECT(Type, out, expr, expectation)
 * @brief Assign a successful value, or abort with `expectation`.
 *
 * Evaluates `expr` once. On failure this calls `result_abort` and does not
 * return. On success `out` receives the value.
 *
 * @param Type Result type from `BD_DEFINE_RESULT`.
 * @param out Destination for the success value.
 * @param expr Expression of type `Type`.
 * @param expectation Note stored with the abort. May be NULL.
 */
#define BD_RESULT_EXPECT(Type, out, expr, expectation)                                 \
    do {                                                                               \
        const Type bd_result_expect_ = (expr);                                         \
        if (!bd_result_expect_.ok) {                                                   \
            result_abort(&bd_result_expect_.error, (expectation), __FILE__, __LINE__); \
        }                                                                              \
        (out) = bd_result_expect_.value;                                               \
    } while (0)

/**
 * @def BD_RESULT_UNWRAP(Type, out, expr)
 * @brief Assign a successful value, or abort.
 *
 * Evaluates `expr` once. On failure this calls `result_abort` and does not
 * return. On success `out` receives the value.
 *
 * @param Type Result type from `BD_DEFINE_RESULT`.
 * @param out Destination for the success value.
 * @param expr Expression of type `Type`.
 */
#define BD_RESULT_UNWRAP(Type, out, expr) \
    BD_RESULT_EXPECT(Type, out, expr, NULL)

/**
 * @def BD_RESULT_UNWRAP_OR(result, fallback)
 * @brief The success value, or `fallback` when `result` failed.
 *
 * `result` is evaluated more than once when it succeeded, so pass a variable.
 * `fallback` is evaluated only on failure.
 *
 * @param result Result object.
 * @param fallback Value used when `result.ok` is false.
 */
#define BD_RESULT_UNWRAP_OR(result, fallback) \
    ((result).ok ? (result).value : (fallback))

#endif /* BEDROCK_RESULT_H */
