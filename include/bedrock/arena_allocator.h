// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#ifndef BEDROCK_ARENA_ALLOCATOR_H
#define BEDROCK_ARENA_ALLOCATOR_H

#include <stddef.h>

typedef struct MemHeap MemHeap;

/**
 * @brief Create an arena with a fixed usable capacity.
 * @param capacity Maximum number of bytes the arena can hold.
 * @return New arena on success, or NULL if capacity is 0 or allocation fails.
 */
MemHeap *arena_create(size_t capacity);

/**
 * @brief Destroy an arena and release all of its memory.
 * @param a Arena to destroy; ignored if NULL.
 */
void arena_destroy(MemHeap *a);

/**
 * @brief Allocate a 16-byte aligned block from the arena.
 * @param a Arena to allocate from.
 * @param n Number of payload bytes requested.
 * @return Pointer to the payload, or NULL if the request cannot be satisfied.
 */
void *arena_alloc(MemHeap *a, size_t n);

/**
 * @brief Return a previously allocated block to the arena.
 *
 * If the free list cannot record the block, it stays allocated and `arena_used`
 * is left unchanged, so the space is not lost.
 * @param a Arena that owns the block.
 * @param p Pointer returned by arena_alloc(); ignored if NULL.
 */
void arena_free(MemHeap *a, void *p);

/**
 * @brief Query the arena's usable capacity.
 * @param a Arena to query.
 * @return Capacity in bytes, or 0 if `a` is NULL.
 */
size_t arena_capacity(const MemHeap *a);

/**
 * @brief Query how many bytes are currently allocated from the arena.
 * @param a Arena to query.
 * @return Bytes in use, or 0 if `a` is NULL.
 */
size_t arena_used(const MemHeap *a);

#endif /* BEDROCK_ARENA_ALLOCATOR_H */
