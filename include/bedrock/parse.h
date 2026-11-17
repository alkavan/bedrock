// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#ifndef BEDROCK_PARSE_H
#define BEDROCK_PARSE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief True when str is non-empty and contains only decimal digits.
 *
 * @param str String to test. NULL is not digits.
 */
bool bd_parse_digits(const char *str);

/**
 * @brief Count ':' characters in str.
 *
 * @param str String to scan. NULL counts as zero.
 */
int bd_parse_colon_count(const char *str);

/**
 * @brief Parse a signed 64-bit decimal integer.
 *
 * Accepts an optional leading `+` or `-`. Rejects empty input, trailing junk,
 * and values outside int64_t.
 * @param str Text to parse.
 * @param out Receives the value on success.
 *
 * @return true on success.
 */
bool bd_parse_i64(const char *str, int64_t *out);

/**
 * @brief Parse an unsigned 64-bit decimal integer. No sign is accepted.
 */
bool bd_parse_u64(const char *str, uint64_t *out);

/**
 * @brief Parse a signed 32-bit decimal integer.
 */
bool bd_parse_i32(const char *str, int32_t *out);

/**
 * @brief Parse an unsigned 32-bit decimal integer.
 */
bool bd_parse_u32(const char *str, uint32_t *out);

/**
 * @brief Parse an unsigned 16-bit decimal integer.
 */
bool bd_parse_u16(const char *str, uint16_t *out);

/**
 * @brief Parse a floating-point value.
 *
 * Uses base-10 text. Rejects empty input, leading space, trailing junk,
 * non-finite values, and overflow.
 */
bool bd_parse_f64(const char *str, double *out);

/**
 * @brief Parse a boolean word.
 *
 * Accepts 1/0, true/false, and yes/no, ignoring case.
 */
bool bd_parse_bool(const char *str, bool *out);

/**
 * @brief Split "host" or "host:port".
 *
 * Several colons stay part of the host. A single trailing ":digits" with a
 * non-zero port is parsed. Rejects empty or leading-dash specs and hosts that
 * do not fit in host_size, including the terminating NUL.
 * @param spec Input "host" or "host:port".
 * @param host Output buffer. Always NUL-terminated on success.
 * @param host_size Capacity of host.
 * @param port Optional. Set only when a valid port is present.
 * @return true on success.
 */
bool bd_parse_host_port(const char *spec, char *host, size_t host_size, uint16_t *port);

#endif /* BEDROCK_PARSE_H */
