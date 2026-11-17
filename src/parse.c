// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "bedrock/parse.h"

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

bool bd_parse_digits(const char *str)
{
    if (!str || str[0] == '\0') {
        return false;
    }

    for (; *str; ++str) {
        if (*str < '0' || *str > '9') {
            return false;
        }
    }

    return true;
}

int bd_parse_colon_count(const char *str)
{
    int count = 0;
    if (!str) {
        return 0;
    }

    for (; *str; ++str) {
        if (*str == ':') {
            count++;
        }
    }

    return count;
}

static bool accumulate(uint64_t *value, const unsigned digit, const uint64_t limit)
{
    if (*value > (limit - digit) / 10u) {
        return false;
    }
    *value = (*value * 10u) + digit;
    return true;
}

bool bd_parse_u64(const char *str, uint64_t *out)
{
    uint64_t value = 0;

    if (!str || str[0] == '\0' || !out) {
        return false;
    }

    for (; *str; ++str) {
        if (*str < '0' || *str > '9') {
            return false;
        }
        if (!accumulate(&value, (unsigned)(*str - '0'), UINT64_MAX)) {
            return false;
        }
    }

    *out = value;
    return true;
}

bool bd_parse_i64(const char *str, int64_t *out)
{
    int sign = 1;
    uint64_t value = 0;

    if (!str || str[0] == '\0' || !out) {
        return false;
    }

    if (*str == '+' || *str == '-') {
        if (*str == '-') {
            sign = -1;
        }
        ++str;
        if (*str == '\0') {
            return false;
        }
    }

    const uint64_t limit = (sign < 0) ? ((uint64_t) INT64_MAX + 1u) : (uint64_t) INT64_MAX;
    for (; *str; ++str) {
        if (*str < '0' || *str > '9') {
            return false;
        }
        if (!accumulate(&value, (unsigned)(*str - '0'), limit)) {
            return false;
        }
    }

    if (sign < 0) {
        if (value == (uint64_t)INT64_MAX + 1u) {
            *out = INT64_MIN;
        } else {
            *out = -(int64_t)value;
        }
    } else {
        *out = (int64_t)value;
    }

    return true;
}

bool bd_parse_i32(const char *str, int32_t *out)
{
    int64_t value = 0;
    if (!out || !bd_parse_i64(str, &value) || value < INT32_MIN || value > INT32_MAX) {
        return false;
    }
    *out = (int32_t)value;
    return true;
}

bool bd_parse_u32(const char *str, uint32_t *out)
{
    uint64_t value = 0;
    if (!out || !bd_parse_u64(str, &value) || value > UINT32_MAX) {
        return false;
    }
    *out = (uint32_t)value;
    return true;
}

bool bd_parse_u16(const char *str, uint16_t *out)
{
    uint64_t value = 0;
    if (!out || !bd_parse_u64(str, &value) || value > UINT16_MAX) {
        return false;
    }
    *out = (uint16_t)value;
    return true;
}

bool bd_parse_f64(const char *str, double *out)
{
    char *end = NULL;

    if (!str || str[0] == '\0' || !out || isspace((unsigned char)str[0])) {
        return false;
    }

    if (strchr(str, 'x') || strchr(str, 'X')) {
        return false;
    }

    errno = 0;
    const double value = strtod(str, &end);
    if (end == str || !end || *end != '\0' || errno == ERANGE) {
        return false;
    }

    if (isnan(value) || value > DBL_MAX || value < -DBL_MAX) {
        return false;
    }

    *out = value;
    return true;
}

bool bd_parse_bool(const char *str, bool *out)
{
    static const char *const yes[] = {"1", "true", "yes", NULL};
    static const char *const no[] = {"0", "false", "no", NULL};
    const char *const *word;
    size_t i;

    if (!str || str[0] == '\0' || !out) {
        return false;
    }

    for (word = yes; *word; ++word) {
        for (i = 0; str[i] && (*word)[i]; ++i) {
            const char left = (char)tolower((unsigned char)str[i]);
            const char right = (char)tolower((unsigned char)(*word)[i]);
            if (left != right) {
                break;
            }
        }

        if (str[i] == '\0' && (*word)[i] == '\0') {
            *out = true;
            return true;
        }
    }

    for (word = no; *word; ++word) {
        for (i = 0; str[i] && (*word)[i]; ++i) {
            const char left = (char)tolower((unsigned char)str[i]);
            const char right = (char)tolower((unsigned char)(*word)[i]);
            if (left != right) {
                break;
            }
        }

        if (str[i] == '\0' && (*word)[i] == '\0') {
            *out = false;
            return true;
        }
    }

    return false;
}

bool bd_parse_host_port(const char *spec, char *host, const size_t host_size, uint16_t *port)
{
    if (!spec || spec[0] == '\0' || spec[0] == '-' || !host || host_size == 0) {
        return false;
    }

    const char *colon = strrchr(spec, ':');
    size_t host_len = strlen(spec);
    if (colon && colon != spec && bd_parse_colon_count(spec) == 1 && bd_parse_digits(colon + 1)) {
        uint16_t parsed = 0;
        host_len = (size_t)(colon - spec);
        if (host_len == 0 || !bd_parse_u16(colon + 1, &parsed) || parsed == 0) {
            return false;
        }

        if (port) {
            *port = parsed;
        }
    }

    if (host_len == 0 || host_len >= host_size) {
        return false;
    }

    memcpy(host, spec, host_len);
    host[host_len] = '\0';
    return true;
}
