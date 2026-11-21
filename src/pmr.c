// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file
 * @brief Polymorphic allocator implementation.
 */

#include "bedrock/pmr.h"

#include <stdlib.h>

#ifdef _WIN32
#  include <malloc.h>
#endif

#if defined(__STDC_NO_ATOMICS__)
#  ifdef _WIN32
#    ifndef WIN32_LEAN_AND_MEAN
#      define WIN32_LEAN_AND_MEAN
#    endif
#    include <windows.h>
#  endif
#else
#  include <stdatomic.h>
#endif

static int pmr_alignment_ok(const size_t alignment)
{
    return alignment != 0 && (alignment & (alignment - 1u)) == 0u;
}

#ifndef _WIN32
/* `alignment` is a non-zero power of two. Returns 0 when the sum would overflow. */
static size_t pmr_align_up(const size_t bytes, const size_t alignment)
{
    if (bytes > SIZE_MAX - (alignment - 1u)) {
        return 0;
    }
    return (bytes + (alignment - 1u)) & ~(alignment - 1u);
}
#endif

int pmr_is_same_object(const PmrResource *self, const PmrResource *other)
{
    return self == other;
}

static void *pmr_overaligned_alloc(const size_t bytes, const size_t alignment)
{
#ifdef _WIN32
    return _aligned_malloc(bytes, alignment);
#else
    const size_t rounded = pmr_align_up(bytes, alignment);
    if (rounded == 0) {
        return NULL;
    }
    return aligned_alloc(alignment, rounded);
#endif
}

static void pmr_overaligned_free(void *p)
{
#ifdef _WIN32
    _aligned_free(p);
#else
    free(p);
#endif
}

static void *pmr_new_delete_allocate(const PmrResource *self, const size_t bytes,
                                     const size_t alignment)
{
    (void)self;
    if (alignment <= _Alignof(max_align_t)) {
        return malloc(bytes);
    }
    return pmr_overaligned_alloc(bytes, alignment);
}

static void pmr_new_delete_deallocate(const PmrResource *self, void *p, const size_t bytes,
                                      const size_t alignment)
{
    (void)self;
    (void)bytes;
    if (alignment <= _Alignof(max_align_t)) {
        free(p);
        return;
    }
    pmr_overaligned_free(p);
}

static void *pmr_null_allocate(const PmrResource *self, const size_t bytes,
                               const size_t alignment)
{
    (void)self;
    (void)bytes;
    (void)alignment;
    return NULL;
}

static void pmr_null_deallocate(const PmrResource *self, void *p,
                                const size_t bytes, const size_t alignment)
{
    (void)self;
    (void)p;
    (void)bytes;
    (void)alignment;
}

static const PmrResourceVtable pmr_new_delete_vtable = {
    .allocate = pmr_new_delete_allocate,
    .deallocate = pmr_new_delete_deallocate,
    .is_equal = pmr_is_same_object
};

static const PmrResourceVtable pmr_null_vtable = {
    .allocate = pmr_null_allocate,
    .deallocate = pmr_null_deallocate,
    .is_equal = pmr_is_same_object
};

static PmrResource pmr_new_delete_instance = {&pmr_new_delete_vtable};
static PmrResource pmr_null_instance = {&pmr_null_vtable};

PmrResource *pmr_new_delete_resource(void)
{
    return &pmr_new_delete_instance;
}

PmrResource *pmr_null_memory_resource(void)
{
    return &pmr_null_instance;
}

void *pmr_allocate(const PmrResource *resource,
                   const size_t bytes, const size_t alignment)
{
    if (resource == NULL
        || resource->vtable == NULL
        || resource->vtable->allocate == NULL) {
        return NULL;
    }

    if (!pmr_alignment_ok(alignment) || bytes == 0) {
        return NULL;
    }

    return resource->vtable->allocate(resource, bytes, alignment);
}

void pmr_deallocate(const PmrResource *resource, void *p,
                    const size_t bytes, const size_t alignment)
{
    if (resource == NULL
        || resource->vtable == NULL
        || resource->vtable->deallocate == NULL) {
        return;
    }

    if (p == NULL || bytes == 0 || !pmr_alignment_ok(alignment)) {
        return;
    }

    resource->vtable->deallocate(resource, p, bytes, alignment);
}

int pmr_is_equal(const PmrResource *a, const PmrResource *b)
{
    if (a == b && a != NULL) {
        return 1;
    }

    if (a == NULL || b == NULL || a->vtable == NULL || a->vtable->is_equal == NULL) {
        return 0;
    }

    return a->vtable->is_equal(a, b) != 0;
}

PmrAllocator pmr_allocator_default(void)
{
    PmrAllocator allocator;
    allocator.resource = pmr_get_default_resource();
    return allocator;
}

PmrAllocator pmr_allocator_from(PmrResource *resource)
{
    PmrAllocator allocator;
    allocator.resource = resource;
    return allocator;
}

PmrResource *pmr_allocator_resource(const PmrAllocator allocator)
{
    return allocator.resource;
}

void *pmr_allocator_allocate(const PmrAllocator allocator, const size_t bytes,
                             const size_t alignment)
{
    return pmr_allocate(allocator.resource, bytes, alignment);
}

void pmr_allocator_deallocate(const PmrAllocator allocator, void *p, const size_t bytes,
                              const size_t alignment)
{
    pmr_deallocate(allocator.resource, p, bytes, alignment);
}

int pmr_allocator_is_equal(const PmrAllocator a, const PmrAllocator b)
{
    if (a.resource == NULL && b.resource == NULL) {
        return 1;
    }
    return pmr_is_equal(a.resource, b.resource);
}

PmrAllocator pmr_allocator_select_on_copy(const PmrAllocator allocator)
{
    (void)allocator;
    return pmr_allocator_default();
}

/*
 * A stored NULL means the default has not been replaced, so get never returns
 * NULL and set(NULL) installs new_delete. The exchange is thread-safe; the
 * resource it points at is not.
 */
#if defined(__STDC_NO_ATOMICS__) && defined(_WIN32)
static void * volatile pmr_default_resource;
#elif defined(__STDC_NO_ATOMICS__)
static PmrResource *pmr_default_resource;
#else
static _Atomic(PmrResource *) pmr_default_resource;
#endif

PmrResource *pmr_get_default_resource(void)
{
#if defined(__STDC_NO_ATOMICS__)
#  ifdef _WIN32
    PmrResource *current = (PmrResource *)InterlockedCompareExchangePointer(
        &pmr_default_resource, NULL, NULL);
#  else
    PmrResource *current = __atomic_load_n(&pmr_default_resource, __ATOMIC_ACQUIRE);
#  endif
#else
    PmrResource *current = atomic_load_explicit(&pmr_default_resource, memory_order_acquire);
#endif
    return current != NULL ? current : pmr_new_delete_resource();
}

PmrResource *pmr_set_default_resource(PmrResource *resource)
{
    PmrResource *installed = resource != NULL ? resource : pmr_new_delete_resource();
#if defined(__STDC_NO_ATOMICS__)
#  ifdef _WIN32
    PmrResource *previous = (PmrResource *)InterlockedExchangePointer(
        &pmr_default_resource, installed);
#  else
    PmrResource *previous = __atomic_exchange_n(&pmr_default_resource, installed, __ATOMIC_ACQ_REL);
#  endif
#else
    PmrResource *previous = atomic_exchange_explicit(&pmr_default_resource,
                                                     installed,
                                                     memory_order_acq_rel);
#endif
    return previous != NULL ? previous : pmr_new_delete_resource();
}
