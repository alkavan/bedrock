// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file
 * @brief Read-only view of a regular file.
 */

#ifndef BEDROCK_FMAP_H
#define BEDROCK_FMAP_H

#include "bedrock/config.h"

#include <stddef.h>
#include <stdint.h>

/**
 * @struct FileMap
 * @brief Read-only view of a file.
 *
 * One view is current. `fmap_data`, `fmap_window`, and `fmap_close` invalidate
 * the pointer from the previous view. The caller does not free that pointer.
 * A `FileMap` is used from one thread. The layout is private.
 */
typedef struct FileMap FileMap;

/**
 * @brief Open a regular file for reading.
 * @param path File path. Passed to `open` or `CreateFileA`.
 * @return New map, or NULL if `path` is NULL or empty, the path is not a
 *         regular file, or the file cannot be opened or sized.
 */
BED_API FileMap *fmap_open(const char *path);

/**
 * @brief Close a map and drop its current view.
 *
 * NULL is ignored.
 *
 * @param map Map returned by `fmap_open`.
 */
BED_API void fmap_close(FileMap *map);

/**
 * @brief File length in bytes.
 * @param map Map to query.
 * @return Length, or 0 if `map` is NULL. An empty file is a successful open
 *         whose size is 0.
 */
BED_API uint64_t fmap_size(const FileMap *map);

/**
 * @brief Map the whole file.
 *
 * On success `*out` is the first byte and `*size` is the length. The bytes are
 * readable. A store is not part of the contract. The pointer is valid until
 * the next `fmap_data`, `fmap_window`, or `fmap_close`.
 *
 * An empty file, a length that does not fit in `size_t`, and an OS that refuses
 * the mapping return -1. That refusal does not read the file into a buffer.
 * On failure `*out` and `*size` are left unchanged.
 *
 * @param map Map returned by `fmap_open`.
 * @param out Receives the view. Unchanged on failure.
 * @param size Receives the view length. Unchanged on failure.
 *
 * @return 0 on success, or -1.
 */
BED_API int fmap_data(FileMap *map, void **out, size_t *size);

/**
 * @brief Map or read `[offset, offset + length)`.
 *
 * On success `*actual` is the number of file bytes available there. It is
 * shorter than `length` only at the end of the file, and `*out` points at the
 * first of those bytes. The pointer addresses `offset` even when `offset` is
 * not aligned to a page. It is valid until the next `fmap_data`,
 * `fmap_window`, or `fmap_close`.
 *
 * `*actual == 0` stores NULL in `*out` and is success when `length` is 0 and
 * `offset <= fmap_size`, or when `offset == fmap_size`. `offset` past the end
 * is -1. On failure `*out` and `*actual` are left unchanged.
 *
 * The OS mapping is tried first. If that fails, one buffer of `*actual` bytes
 * is read. The read is never the size of the whole file.
 *
 * @param map Map returned by `fmap_open`.
 * @param offset First file byte of the view.
 * @param length Requested length. Shortened at the end of the file.
 * @param out Receives the view. Unchanged on failure.
 * @param actual Receives the byte count. Unchanged on failure.
 *
 * @return 0 on success, or -1.
 */
BED_API int fmap_window(FileMap *map, uint64_t offset, size_t length,
                       void **out, size_t *actual);

#endif /* BEDROCK_FMAP_H */
