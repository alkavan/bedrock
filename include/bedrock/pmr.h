// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file
 * @brief Polymorphic memory resources and allocator handles.
 */

#ifndef BEDROCK_PMR_H
#define BEDROCK_PMR_H

#include "bedrock/config.h"

#include <stddef.h>
#include <stdint.h>

/*
 * MSVC's C <stddef.h> does not declare max_align_t, so _Alignof(max_align_t)
 * is a syntax error (C2061). The missing type is the greatest fundamental
 * alignment, which is the bound between malloc and the aligned allocator.
 * A union of the scalar types is that bound on every MSVC ABI. Compilers
 * that already provide max_align_t are left alone.
 */
#if defined(_MSC_VER) && !defined(__clang__) && !defined(__cplusplus)
typedef union {
    long long pmr_max_align_ll;
    long double pmr_max_align_ld;
    double pmr_max_align_d;
    void *pmr_max_align_p;
} max_align_t;
#endif

/**
 * @struct PmrResource
 * @brief Polymorphic memory resource.
 *
 * Embed this as the first member of a custom resource, then cast a
 * `PmrResource *` back to the derived pointer. The vtable has three slots.
 * A later optional slot may be appended only when NULL means "unsupported".
 */
typedef struct PmrResource PmrResource;

/**
 * @brief Operations implemented by a memory resource.
 *
 * The wrappers have already rejected a null resource, a zero size, and an
 * alignment that is not a non-zero power of two. `allocate` returns an
 * aligned block of at least `bytes` uninitialized bytes, or NULL on failure.
 * `deallocate` does not report failure. `is_equal` is true only when both
 * resources can free each other's blocks. Do not call `pmr_is_equal` on
 * `self` from inside `is_equal`; that recurses.
 */
typedef struct PmrResourceVtable {
    void *(*allocate)(const PmrResource *self, size_t bytes, size_t alignment);
    void (*deallocate)(const PmrResource *self, void *p, size_t bytes, size_t alignment);
    int (*is_equal)(const PmrResource *self, const PmrResource *other);
} PmrResourceVtable;

struct PmrResource {
    const PmrResourceVtable *vtable; /**< Operations for this resource. Never NULL. */
};

/**
 * @brief Allocate at least `bytes` aligned to `alignment`.
 *
 * @param resource Resource to allocate from.
 * @param bytes Number of bytes. Zero returns NULL without calling the vtable;
 *        that NULL is success, because the caller knows the size it passed.
 * @param alignment Required alignment. Must be a non-zero power of two.
 *        Zero is an error, not a synonym for `max_align_t`.
 *
 * @return Aligned pointer, or NULL if the arguments are invalid, `bytes` is 0,
 *         or the resource cannot obtain the block.
 */
BED_API void *pmr_allocate(const PmrResource *resource, size_t bytes, size_t alignment);

/**
 * @brief Release a block returned by `pmr_allocate` on an equal resource.
 *
 * `bytes` and `alignment` are the values passed to the matching allocate.
 * A null resource, a null pointer, a zero size, or a bad alignment returns
 * without calling the vtable.
 *
 * @param resource Resource that compares equal to the one that allocated `p`.
 * @param p Block to release. NULL is ignored.
 * @param bytes Size passed to allocate.
 * @param alignment Alignment passed to allocate.
 */
BED_API void pmr_deallocate(const PmrResource *resource, void *p, size_t bytes, size_t alignment);

/**
 * @brief Whether blocks from `a` can be freed on `b`.
 *
 * Equal addresses are equal. Otherwise, only `a`'s `is_equal` is called.
 *
 * @param a Resource that allocated the block.
 * @param b Resource that would free it.
 * 
 * @return Non-zero when the resources are equal. A null argument is not equal.
 */
BED_API int pmr_is_equal(const PmrResource *a, const PmrResource *b);

/**
 * @brief Identity comparison for a resource vtable.
 *
 * @param self Resource being asked.
 * @param other Resource to compare.
 * 
 * @return Non-zero when `self` and `other` are the same object.
 */
BED_API int pmr_is_same_object(const PmrResource *self, const PmrResource *other);

/**
 * @brief Handle stored by a container.
 *
 * Copying the struct copies the pointer. It does not allocate or free.
 * Assignment copies the pointer and frees nothing. There is no destructor.
 */
typedef struct PmrAllocator {
    PmrResource *resource;
} PmrAllocator;

/**
 * @brief Handle for the current default resource.
 *
 * Later changes to the default do not retarget the returned handle.
 *
 * @return Handle for `pmr_get_default_resource()` at this call.
 */
BED_API PmrAllocator pmr_allocator_default(void);

/**
 * @brief Handle that stores `resource` as given.
 *
 * NULL stays NULL. It is not replaced with the default resource or the null
 * singleton. Allocations through a null handle fail.
 *
 * @param resource Resource to store. NULL stays NULL.
 *
 * @return Handle that stores `resource` as given.
 */
BED_API PmrAllocator pmr_allocator_from(PmrResource *resource);

/**
 * @brief Resource stored in the handle.
 * @param allocator Handle to query.
 * @return Resource stored in the handle. May be NULL.
 */
BED_API PmrResource *pmr_allocator_resource(PmrAllocator allocator);

/**
 * @brief `pmr_allocate` through the handle.
 *
 * There is no default alignment. Callers that want the C default pass
 * `_Alignof(max_align_t)`.
 *
 * @param allocator Handle to allocate through.
 * @param bytes Number of bytes. Zero returns NULL.
 * @param alignment Required alignment. Must be a non-zero power of two.
 *
 * @return Aligned pointer, or NULL if allocation fails.
 */
BED_API void *pmr_allocator_allocate(PmrAllocator allocator, size_t bytes, size_t alignment);

/**
 * @brief `pmr_deallocate` through the handle.
 *
 * @param allocator Handle that allocated the block.
 * @param p Block to release. NULL is ignored.
 * @param bytes Size passed to allocate.
 * @param alignment Alignment passed to allocate.
 */
BED_API void pmr_allocator_deallocate(PmrAllocator allocator, void *p, size_t bytes, size_t alignment);

/**
 * @brief Whether the two handles can free each other's blocks.
 *
 * This is resource equality. Two null handles compare equal.
 *
 * @param a First handle.
 * @param b Second handle.
 *
 * @return Non-zero when the handles can free each other's blocks.
 */
BED_API int pmr_allocator_is_equal(PmrAllocator a, PmrAllocator b);

/**
 * @brief Handle for the default resource at this call.
 *
 * The argument is ignored. Container copy does not call this implicitly;
 * a caller that wants the C++ rule passes the result itself.
 *
 * @param allocator Ignored. Present so container copy can pass the source handle.
 *
 * @return Handle for the default resource at this call.
 */
BED_API PmrAllocator pmr_allocator_select_on_copy(PmrAllocator allocator);

/**
 * @brief Allocate `count` elements, or NULL if the product overflows.
 *
 * @param allocator Handle to allocate through.
 * @param count Number of elements. Zero returns NULL.
 * @param elem_size Size of one element in bytes.
 * @param elem_align Alignment of one element. `_Alignof(T)` at the call site.
 *
 * @return Aligned array, or NULL if the product overflows or allocation fails.
 */
static inline void *pmr_allocator_allocate_array(const PmrAllocator allocator,
                                                 const size_t count,
                                                 const size_t elem_size,
                                                 const size_t elem_align) {
    if (elem_size != 0 && count > SIZE_MAX / elem_size) {
        return NULL;
    }

    return pmr_allocator_allocate(allocator, count * elem_size, elem_align);
}

/**
 * @brief Release an array allocated by `pmr_allocator_allocate_array`.
 *
 * A null pointer, a zero element size, or a product that overflows returns
 * without deallocating.
 *
 * @param allocator Handle that allocated the array.
 * @param p Array to release. NULL is ignored.
 * @param count Element count passed to allocate.
 * @param elem_size Element size passed to allocate.
 * @param elem_align Element alignment passed to allocate.
 */
static inline void pmr_allocator_deallocate_array(const PmrAllocator allocator,
                                                  void *p,
                                                  const size_t count,
                                                  const size_t elem_size,
                                                  const size_t elem_align)
{
    if (p == NULL || elem_size == 0 || count > SIZE_MAX / elem_size) {
        return;
    }
    pmr_allocator_deallocate(allocator, p, count * elem_size, elem_align);
}

/**
 * @brief Process-lifetime resource backed by `malloc` and `free`.
 *
 * The same address is returned every time and is never freed. Alignment at
 * most `_Alignof(max_align_t)` uses `malloc` / `free`. A stricter alignment
 * uses `_aligned_malloc` / `_aligned_free` on Windows and `aligned_alloc` /
 * `free` otherwise. `deallocate` selects that free from `alignment`, so the
 * caller passes the same alignment it passed to `allocate`. Safe to call
 * from any thread.
 *
 * @return The process-lifetime resource. The same address is returned every time.
 */
BED_API PmrResource *pmr_new_delete_resource(void);

/**
 * @brief Process-lifetime resource whose allocate always fails.
 *
 * `deallocate` does nothing. The same address is returned every time.
 * Pass this as an upstream to forbid growth. Safe to call from any thread.
 *
 * @return The process-lifetime resource. The same address is returned every time.
 */
BED_API PmrResource *pmr_null_memory_resource(void);

/**
 * @brief Process-wide default resource.
 *
 * The initial value is `pmr_new_delete_resource()`. Never returns NULL.
 * Thread-safe. The resource itself is not made thread-safe by this call.
 *
 * @return The current default resource. Never NULL.
 */
BED_API PmrResource *pmr_get_default_resource(void);

/**
 * @brief Install the process-wide default resource and return the previous one.
 *
 * NULL installs `pmr_new_delete_resource()`. The caller keeps `resource`
 * alive for as long as it stays installed and for as long as any handle
 * still stores it. Installing it does not extend its lifetime. Thread-safe.
 * 
 * @param resource Resource to install as the process-wide default. NULL
 *        installs `pmr_new_delete_resource()`.
 *        
 * @return Previous default resource. Never NULL.
 */
BED_API PmrResource *pmr_set_default_resource(PmrResource *resource);

#endif /* BEDROCK_PMR_H */
