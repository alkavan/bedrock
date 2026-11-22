// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file
 * @brief Fixed-capacity arena that can free individual blocks.
 */

#ifndef BEDROCK_ARENA_H
#define BEDROCK_ARENA_H

#include "bedrock/config.h"

#include <stddef.h>

/**
 * @struct MemHeap
 * @brief Fixed-capacity arena.
 *
 * Create one with `arena_create`. The layout is private.
 */
typedef struct MemHeap MemHeap;

/**
 * @brief Create an arena with a fixed usable capacity.
 * @param capacity Maximum number of bytes the arena can hold.
 * @return New arena on success, or NULL if capacity is 0 or allocation fails.
 */
BED_API MemHeap *arena_create(size_t capacity);

/**
 * @brief Destroy an arena and release all of its memory.
 * @param a Arena to destroy; ignored if NULL.
 */
BED_API void arena_destroy(MemHeap *a);

/**
 * @brief Discard every live block and keep the arena's mapping.
 *
 * The cost follows the free-node list, not the number of live blocks. An arena
 * that has only been allocated from has a single free node, so the reset is
 * constant time. Pages already touched stay committed.
 *
 * @param a Arena to reset; ignored if NULL.
 */
BED_API void arena_reset(MemHeap *a);

/**
 * @brief Allocate a 16-byte aligned block from the arena.
 *
 * @param a Arena to allocate from.
 * @param n Number of payload bytes requested.
 *
 * @return Pointer to the payload, or NULL if the request cannot be satisfied.
 */
BED_API void *arena_alloc(MemHeap *a, size_t n);

/**
 * @brief Allocate a block aligned to at least `alignment`.
 *
 * `alignment` must be a power of two. The payload is aligned to the greater
 * of `alignment` and 16. Padding used to reach that alignment stays inside
 * the block and is reclaimed by `arena_free`.
 *
 * @param a Arena to allocate from.
 * @param n Number of payload bytes requested.
 * @param alignment Required alignment. Zero and non-powers of two are rejected.
 *
 * @return Pointer to the payload, or NULL if the request cannot be satisfied.
 */
BED_API void *arena_alloc_aligned(MemHeap *a, size_t n, size_t alignment);

/**
 * @brief Return a previously allocated block to the arena.
 *
 * If the free list cannot record the block, it stays allocated and `arena_used`
 * is left unchanged, so the space is not lost.
 *
 * @param a Arena that owns the block.
 * @param p Pointer returned by arena_alloc(); ignored if NULL.
 */
BED_API void arena_free(MemHeap *a, void *p);

/**
 * @brief Query the arena's usable capacity.
 * @param a Arena to query.
 * @return Capacity in bytes, or 0 if `a` is NULL.
 */
BED_API size_t arena_capacity(const MemHeap *a);

/**
 * @brief Query how many bytes are currently allocated from the arena.
 * @param a Arena to query.
 * @return Bytes in use, or 0 if `a` is NULL.
 */
BED_API size_t arena_used(const MemHeap *a);

#endif /* BEDROCK_ARENA_H */
