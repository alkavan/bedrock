// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#include <bedrock/parse.h>
#include <bedrock/unitest.h>

#include <stdint.h>
#include <string.h>

TEST_CASE(test_parse_integers, "[parse]") {
    int64_t i64 = 0;
    uint64_t u64 = 0;
    int32_t i32 = 0;
    uint32_t u32 = 0;
    uint16_t u16 = 0;

    REQUIRE(bd_parse_u64("42", &u64));
    REQUIRE(u64 == 42u);
    REQUIRE(bd_parse_i64("-7", &i64));
    REQUIRE(i64 == -7);
    REQUIRE(bd_parse_i64("+8", &i64));
    REQUIRE(i64 == 8);
    REQUIRE(bd_parse_i64("0", &i64));
    REQUIRE(i64 == 0);
    REQUIRE(bd_parse_u16("65535", &u16));
    REQUIRE(u16 == 65535u);
    REQUIRE(bd_parse_i32("-2147483648", &i32));
    REQUIRE(i32 == INT32_MIN);
    REQUIRE(bd_parse_u32("4294967295", &u32));
    REQUIRE(u32 == UINT32_MAX);
    REQUIRE(bd_parse_i64("-9223372036854775808", &i64));
    REQUIRE(i64 == INT64_MIN);
    REQUIRE(bd_parse_u64("18446744073709551615", &u64));
    REQUIRE(u64 == UINT64_MAX);
}

TEST_CASE(test_parse_integers_reject, "[parse]") {
    int64_t i64 = 1;
    uint64_t u64 = 1;
    uint16_t u16 = 1;

    REQUIRE(!bd_parse_u64(NULL, &u64));
    REQUIRE(!bd_parse_u64("", &u64));
    REQUIRE(!bd_parse_u64("12a", &u64));
    REQUIRE(!bd_parse_u64("+3", &u64));
    REQUIRE(!bd_parse_u64("-3", &u64));
    REQUIRE(!bd_parse_u64("18446744073709551616", &u64));
    REQUIRE(!bd_parse_i64("-", &i64));
    REQUIRE(!bd_parse_i64("9223372036854775808", &i64));
    REQUIRE(!bd_parse_i64("-9223372036854775809", &i64));
    REQUIRE(!bd_parse_u16("65536", &u16));
    REQUIRE(u64 == 1u);
    REQUIRE(i64 == 1);
    REQUIRE(u16 == 1u);
    REQUIRE(!bd_parse_digits(""));
    REQUIRE(!bd_parse_digits("12a"));
    REQUIRE(bd_parse_digits("007"));
    REQUIRE(bd_parse_colon_count(NULL) == 0);
    REQUIRE(bd_parse_colon_count("a:b:c") == 2);
}

TEST_CASE(test_parse_bool_and_float, "[parse]") {
    bool flag = false;
    double number = 0.0;

    REQUIRE(bd_parse_bool("TRUE", &flag) && flag);
    REQUIRE(bd_parse_bool("yes", &flag) && flag);
    REQUIRE(bd_parse_bool("1", &flag) && flag);
    REQUIRE(bd_parse_bool("No", &flag) && !flag);
    REQUIRE(bd_parse_bool("false", &flag) && !flag);
    REQUIRE(bd_parse_bool("0", &flag) && !flag);
    REQUIRE(!bd_parse_bool("truth", &flag));
    REQUIRE(!bd_parse_bool("2", &flag));

    REQUIRE(bd_parse_f64("1.5", &number));
    REQUIRE(number > 1.49 && number < 1.51);
    REQUIRE(bd_parse_f64("-2e1", &number));
    REQUIRE(number > -20.01 && number < -19.99);
    REQUIRE(!bd_parse_f64("1.5x", &number));
    REQUIRE(!bd_parse_f64("", &number));
    REQUIRE(!bd_parse_f64(" 1", &number));
    REQUIRE(!bd_parse_f64("inf", &number));
    REQUIRE(!bd_parse_f64("nan", &number));
}

TEST_CASE(test_parse_host_port, "[parse]") {
    char host[32];
    uint16_t port = 9;

    REQUIRE(bd_parse_host_port("localhost", host, sizeof(host), &port));
    REQUIRE(strcmp(host, "localhost") == 0);
    REQUIRE(port == 9u);

    REQUIRE(bd_parse_host_port("localhost:80", host, sizeof(host), &port));
    REQUIRE(strcmp(host, "localhost") == 0);
    REQUIRE(port == 80u);
    REQUIRE(!bd_parse_host_port("localhost:0", host, sizeof(host), &port));
    REQUIRE(!bd_parse_host_port("-bad", host, sizeof(host), &port));
    REQUIRE(!bd_parse_host_port("", host, sizeof(host), &port));

    REQUIRE(bd_parse_host_port("a:b:c", host, sizeof(host), &port));
    REQUIRE(strcmp(host, "a:b:c") == 0);
    REQUIRE(port == 80u);
}
