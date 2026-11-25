// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#include <bedrock/result.h>
#include <bedrock/unitest.h>

#include <string.h>

typedef struct SamplePoint {
    int x;
    int y;
} SamplePoint;

BD_DEFINE_RESULT(IntResult, int)
BD_DEFINE_RESULT(PointResult, SamplePoint)
BD_DEFINE_RESULT(TextResult, const char *)

static int succeed_calls = 0;
static int fallback_calls = 0;

static IntResult succeed(const int value)
{
    succeed_calls += 1;
    return BD_RESULT_OK(IntResult, value);
}

static IntResult fail_parse(int *line)
{
    const int at = __LINE__ + 1;
    const ResultError error = BD_RESULT_ERROR(4, "parse", "bad text");

    if (line != NULL) {
        *line = at;
    }
    return BD_RESULT_ERR(IntResult, error);
}

static IntResult through_one(const int fail)
{
    int value = 7;

    if (fail) {
        BD_RESULT_TRY(IntResult, value, fail_parse(NULL));
    }
    BD_RESULT_TRY(IntResult, value, succeed(value));
    return BD_RESULT_OK(IntResult, value + 1);
}

static IntResult through_two(const int fail)
{
    int value = 0;

    BD_RESULT_TRY(IntResult, value, through_one(fail));
    return BD_RESULT_OK(IntResult, value + 1);
}

static IntResult assign_out(int *out, const int fail)
{
    if (fail) {
        BD_RESULT_TRY(IntResult, *out, fail_parse(NULL));
    } else {
        BD_RESULT_TRY(IntResult, *out, succeed(5));
    }
    return BD_RESULT_OK(IntResult, *out);
}

static int count_fallback(void)
{
    fallback_calls += 1;
    return -5;
}

static PointResult make_point(void)
{
    SamplePoint point;

    point.x = 2;
    point.y = 5;
    return BD_RESULT_OK(PointResult, point);
}

static PointResult fail_point(void)
{
    return BD_RESULT_ERR(PointResult, BD_RESULT_ERROR(6, "point", "missing"));
}

static TextResult make_text(void)
{
    return BD_RESULT_OK(TextResult, "ok");
}

static TextResult fail_text(void)
{
    return BD_RESULT_ERR(TextResult, BD_RESULT_ERROR(8, "empty", "missing"));
}

TEST_CASE(test_result_ok_and_err, "[result]") {
    const IntResult ok = succeed(11);
    int line = 0;
    const IntResult err = fail_parse(&line);

    REQUIRE(BD_RESULT_IS_OK(ok));
    REQUIRE(!BD_RESULT_IS_ERR(ok));
    REQUIRE(ok.value == 11);
    REQUIRE(ok.error.code == 0);
    REQUIRE(ok.error.name == NULL);
    REQUIRE(ok.error.message == NULL);
    REQUIRE(ok.error.file == NULL);
    REQUIRE(ok.error.line == 0);

    REQUIRE(BD_RESULT_IS_ERR(err));
    REQUIRE(!BD_RESULT_IS_OK(err));
    REQUIRE(err.value == 0);
    REQUIRE(err.error.code == 4);
    REQUIRE(strcmp(err.error.name, "parse") == 0);
    REQUIRE(strcmp(err.error.message, "bad text") == 0);
    REQUIRE(strcmp(err.error.file, __FILE__) == 0);
    REQUIRE(err.error.line == line);
    REQUIRE(line > 0);

    const ResultError made = result_error(3, "range", "too big", "demo.c", 12);
    REQUIRE(made.code == 3);
    REQUIRE(strcmp(made.name, "range") == 0);
    REQUIRE(strcmp(made.message, "too big") == 0);
    REQUIRE(strcmp(made.file, "demo.c") == 0);
    REQUIRE(made.line == 12);
}

TEST_CASE(test_result_try_propagates, "[result]") {
    int out = 42;

    succeed_calls = 0;
    IntResult failed = through_two(1);
    REQUIRE(BD_RESULT_IS_ERR(failed));
    REQUIRE(failed.error.code == 4);
    REQUIRE(strcmp(failed.error.name, "parse") == 0);
    REQUIRE(strcmp(failed.error.message, "bad text") == 0);
    REQUIRE(strcmp(failed.error.file, __FILE__) == 0);
    REQUIRE(succeed_calls == 0);

    succeed_calls = 0;
    IntResult passed = through_two(0);
    REQUIRE(BD_RESULT_IS_OK(passed));
    REQUIRE(passed.value == 9);
    REQUIRE(succeed_calls == 1);

    out = 42;
    failed = assign_out(&out, 1);
    REQUIRE(BD_RESULT_IS_ERR(failed));
    REQUIRE(out == 42);

    out = 42;
    passed = assign_out(&out, 0);
    REQUIRE(BD_RESULT_IS_OK(passed));
    REQUIRE(out == 5);
    REQUIRE(passed.value == 5);
}

TEST_CASE(test_result_unwrap_and_fallback, "[result]") {
    int value = 0;
    const IntResult ok = succeed(6);
    const IntResult err = fail_parse(NULL);
    const PointResult point = make_point();
    const PointResult missing = fail_point();
    const TextResult text = make_text();
    const TextResult empty = fail_text();

    BD_RESULT_UNWRAP(IntResult, value, succeed(9));
    REQUIRE(value == 9);
    BD_RESULT_EXPECT(IntResult, value, succeed(4), "digit");
    REQUIRE(value == 4);

    fallback_calls = 0;
    REQUIRE(BD_RESULT_UNWRAP_OR(ok, count_fallback()) == 6);
    REQUIRE(fallback_calls == 0);
    REQUIRE(BD_RESULT_UNWRAP_OR(err, count_fallback()) == -5);
    REQUIRE(fallback_calls == 1);

    REQUIRE(point.ok);
    REQUIRE(point.value.x == 2);
    REQUIRE(point.value.y == 5);
    REQUIRE(point.error.code == 0);
    REQUIRE(!missing.ok);
    REQUIRE(missing.value.x == 0);
    REQUIRE(missing.value.y == 0);
    REQUIRE(missing.error.code == 6);

    REQUIRE(text.ok);
    REQUIRE(strcmp(text.value, "ok") == 0);
    REQUIRE(!empty.ok);
    REQUIRE(empty.value == NULL);
    REQUIRE(empty.error.code == 8);
}
