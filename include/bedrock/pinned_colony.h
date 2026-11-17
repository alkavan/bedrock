// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#ifndef BEDROCK_PINNED_COLONY_H
#define BEDROCK_PINNED_COLONY_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Fixed-capacity colony.
 *
 * Element addresses stay valid until that element is erased, or the colony
 * is cleared or destroyed. Erased slots are reused. Capacity never grows.
 * Iteration visits live elements in increasing index order and skips holes.
 */
typedef struct PinnedColony PinnedColony;

/**
 * @brief Called once for a live element on erase, clear, or destroy.
 *
 * @param element The slot that is about to be released.
 * @param user The pointer passed to `pinned_colony_create`.
 */
typedef void (*PinnedColonyDtor)(void *element, void *user);

/**
 * @brief Create a colony with a fixed number of slots.
 *
 * @param capacity Maximum number of live elements. Zero is an empty colony.
 * @param elem_size Size of one element in bytes. Must be non-zero.
 * @param elem_align Alignment of each slot. Zero means 1. Must be a power of
 *        two no greater than the system page size.
 * @param dtor Optional destructor, or NULL.
 * @param user Pointer passed through to `dtor`.
 * @return New colony, or NULL if the arguments are invalid or memory cannot be reserved.
 */
PinnedColony *pinned_colony_create(size_t capacity, size_t elem_size, size_t elem_align,
                                    PinnedColonyDtor dtor, void *user);

/**
 * @brief Destroy a colony, run destructors for live elements, and release its memory.
 *
 * @param colony Colony to destroy. NULL is ignored.
 */
void pinned_colony_destroy(PinnedColony *colony);

/**
 * @brief Take an uninitialized live slot.
 *
 * @param colony Colony to allocate from.
 * @return Pointer to the slot, or NULL if `colony` is NULL, it is full, or pages cannot be committed.
 */
void *pinned_colony_acquire(PinnedColony *colony);

/**
 * @brief Copy `elem_size` bytes into a new live slot.
 *
 * @param colony Colony to insert into.
 * @param src Bytes to copy. NULL is rejected.
 * @return Pointer to the new element, or NULL on failure.
 */
void *pinned_colony_insert(PinnedColony *colony, const void *src);

/**
 * @brief Destroy one live element and return its slot to the free list.
 *
 * @param colony Colony that owns the element.
 * @param element Pointer previously returned by acquire, insert, or at.
 * @return 0 on success, or -1 if `element` is not a live element of `colony`.
 */
int pinned_colony_erase(PinnedColony *colony, const void *element);

/**
 * @brief Destroy every live element and make those slots reusable.
 *
 * @param colony Colony to clear. NULL is ignored.
 */
void pinned_colony_clear(PinnedColony *colony);

/**
 * @brief Live element count.
 *
 * @return Number of live elements, or 0 if `colony` is NULL.
 */
size_t pinned_colony_size(const PinnedColony *colony);

/**
 * @brief Maximum slot capacity.
 *
 * @return Slot capacity, or 0 if `colony` is NULL.
 */
size_t pinned_colony_capacity(const PinnedColony *colony);

/**
 * @brief Whether the colony has no live elements.
 *
 * @return Non-zero when there are no live elements. A NULL colony is empty.
 */
int pinned_colony_empty(const PinnedColony *colony);

/**
 * @brief Pointer to a live element.
 *
 * @param colony Colony to query.
 * @param index Slot index.
 * @return Element pointer, or NULL if the index is not live.
 */
void *pinned_colony_at(PinnedColony *colony, size_t index);

/**
 * @brief Next live index after `index`, in increasing order.
 *
 * Pass `SIZE_MAX` to get the first live index. The end of the walk is `SIZE_MAX`.
 */
size_t pinned_colony_next_index(const PinnedColony *colony, size_t index);

/**
 * @brief Previous live index before `index`.
 *
 * Pass `SIZE_MAX` to get the last live index. The beginning of the walk is `SIZE_MAX`.
 */
size_t pinned_colony_prev_index(const PinnedColony *colony, size_t index);

/**
 * @brief Slot index of a live element pointer.
 *
 * @return Index, or `SIZE_MAX` if `element` is not an aligned live element of `colony`.
 */
size_t pinned_colony_index(const PinnedColony *colony, const void *element);

#endif /* BEDROCK_PINNED_COLONY_H */
