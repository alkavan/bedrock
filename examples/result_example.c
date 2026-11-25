// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#include <bedrock/result.h>

#include <stdio.h>
#include <stdlib.h>

BD_DEFINE_RESULT(Parsed, int)

static Parsed parse_digit(const char *text)
{
    if (text == NULL || text[0] < '0' || text[0] > '9' || text[1] != '\0') {
        return BD_RESULT_ERR(Parsed, BD_RESULT_ERROR(1, "parse", "expected one digit"));
    }

    return BD_RESULT_OK(Parsed, text[0] - '0');
}

static Parsed add_digits(const char *left, const char *right)
{
    int a = 0;
    int b = 0;

    BD_RESULT_TRY(Parsed, a, parse_digit(left));
    BD_RESULT_TRY(Parsed, b, parse_digit(right));
    return BD_RESULT_OK(Parsed, a + b);
}

int main(void)
{
    const Parsed sum = add_digits("2", "3");
    const Parsed bad = add_digits("2", "x");
    int shown = 0;
    int fallback = 0;

    if (!sum.ok || sum.value != 5 || sum.error.code != 0) {
        fprintf(stderr, "result example: expected 2 + 3 = 5\n");
        return EXIT_FAILURE;
    }

    BD_RESULT_UNWRAP(Parsed, shown, sum);
    BD_RESULT_EXPECT(Parsed, shown, sum, "2 + 3");
    if (shown != 5) {
        fprintf(stderr, "result example: unwrap lost the sum\n");
        return EXIT_FAILURE;
    }

    if (bad.ok || bad.error.code != 1 || bad.value != 0) {
        fprintf(stderr, "result example: expected a parse error\n");
        return EXIT_FAILURE;
    }

    fallback = BD_RESULT_UNWRAP_OR(bad, -1);
    if (fallback != -1) {
        fprintf(stderr, "result example: expected the fallback value\n");
        return EXIT_FAILURE;
    }

    printf("result example: 2 + 3 = %d\n", sum.value);
    printf("result example: 2 + x -> %s (%d): %s\n",
           bad.error.name, bad.error.code, bad.error.message);
    return EXIT_SUCCESS;
}
