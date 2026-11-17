// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#ifndef BEDROCK_H
#define BEDROCK_H

// Version
#define BEDROCK_VERSION_MAJOR 1
#define BEDROCK_VERSION_MINOR 0
#define BEDROCK_VERSION_PATCH 0

// Version macros
#define BEDROCK_STR_HELPER(x) #x
#define BEDROCK_STR(x) BEDROCK_STR_HELPER(x)
#define BEDROCK_VERSION BEDROCK_STR(BEDROCK_VERSION_MAJOR) "." \
    BEDROCK_STR(BEDROCK_VERSION_MINOR) "." BEDROCK_STR(BEDROCK_VERSION_PATCH)

#endif /* BEDROCK_H */
