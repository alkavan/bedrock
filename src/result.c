// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file
 * @brief Abort path for a failed result check.
 */

#include "bedrock/result.h"

#include <stdio.h>
#include <stdlib.h>

_Noreturn void result_abort(const ResultError *error, const char *expectation,
                            const char *file, const int line)
{
    const int code = (error != NULL) ? error->code : -1;
    const char *name = (error != NULL && error->name != NULL) ? error->name : "error";
    const char *message = (error != NULL && error->message != NULL) ? error->message : "";
    const char *origin = (error != NULL && error->file != NULL) ? error->file : "";
    const int origin_line = (error != NULL) ? error->line : 0;
    const char *where = (file != NULL) ? file : "";
    const char *note = (expectation != NULL) ? expectation : "";

    if (note[0] == '\0') {
        fprintf(stderr, "%s (%d): %s at %s:%d; aborted at %s:%d\n",
                name, code, message, origin, origin_line, where, line);
    } else {
        fprintf(stderr, "%s (%d): %s (%s) at %s:%d; aborted at %s:%d\n",
                name, code, message, note, origin, origin_line, where, line);
    }

    abort();
}
