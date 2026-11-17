// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#include <bedrock/pmr.h>
#include <bedrock/unitest.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct CountingResource {
    PmrResource base;
    PmrResource *upstream;
    size_t live_blocks;
    size_t live_bytes;
    size_t allocate_calls;
    size_t deallocate_calls;
} CountingResource;

static void *counting_allocate(const PmrResource *self,
                               const size_t bytes, const size_t alignment) {
    CountingResource *counting = (CountingResource *)self;
    counting->allocate_calls += 1;

    void *block = pmr_allocate(counting->upstream, bytes, alignment);
    if (block == NULL) {
        return NULL;
    }

    counting->live_blocks += 1;
    counting->live_bytes += bytes;

    return block;
}

static void counting_deallocate(const PmrResource *self, void *p,
                                const size_t bytes, const size_t alignment) {
    CountingResource *counting = (CountingResource *)self;
    counting->deallocate_calls += 1;
    pmr_deallocate(counting->upstream, p, bytes, alignment);
    counting->live_blocks -= 1;
    counting->live_bytes -= bytes;
}

static const PmrResourceVtable counting_vtable = {
    .allocate = counting_allocate,
    .deallocate = counting_deallocate,
    .is_equal = pmr_is_same_object
};

static void counting_init(CountingResource *counting, PmrResource *upstream) {
    counting->base.vtable = &counting_vtable;
    counting->upstream = upstream;
    counting->live_blocks = 0;
    counting->live_bytes = 0;
    counting->allocate_calls = 0;
    counting->deallocate_calls = 0;
}

static void *always_null_allocate(const PmrResource *self,
                                  const size_t bytes, const size_t alignment) {
    (void)self;
    (void)bytes;
    (void)alignment;
    return NULL;
}

static void always_null_deallocate(const PmrResource *self, void *p,
                                   const size_t bytes, const size_t alignment) {
    (void)self;
    (void)p;
    (void)bytes;
    (void)alignment;
}

static const PmrResourceVtable always_null_vtable = {
    .allocate = always_null_allocate,
    .deallocate = always_null_deallocate,
    .is_equal = pmr_is_same_object
};

static int never_equal(const PmrResource *self, const PmrResource *other) {
    (void)self;
    (void)other;
    return 0;
}

static int always_equal(const PmrResource *self, const PmrResource *other) {
    (void)self;
    (void)other;
    return 1;
}

static const PmrResourceVtable never_equal_vtable = {
    .allocate = always_null_allocate,
    .deallocate = always_null_deallocate,
    .is_equal = never_equal
};

static const PmrResourceVtable always_equal_vtable = {
    .allocate = always_null_allocate,
    .deallocate = always_null_deallocate,
    .is_equal = always_equal
};

TEST_CASE(test_pmr_reject_bad_arguments, "[pmr]") {
    int stack_object = 0;
    const PmrResource missing_vtable = {0};
    PmrResourceVtable missing_allocate = {0};
    const PmrResource missing_function = {&missing_allocate};

    REQUIRE(pmr_allocate(NULL, 8, 8) == NULL);
    REQUIRE(pmr_allocate(&missing_vtable, 8, 8) == NULL);
    REQUIRE(pmr_allocate(&missing_function, 8, 8) == NULL);
    REQUIRE(pmr_allocate(pmr_new_delete_resource(), 8, 0) == NULL);
    REQUIRE(pmr_allocate(pmr_new_delete_resource(), 8, 3) == NULL);

    const PmrAllocator empty = pmr_allocator_from(NULL);
    REQUIRE(pmr_allocator_resource(empty) == NULL);
    REQUIRE(pmr_allocator_allocate(empty, 8, 8) == NULL);
    REQUIRE(pmr_allocator_allocate(pmr_allocator_from(pmr_new_delete_resource()), 8, 0) == NULL);

    pmr_deallocate(NULL, &stack_object, 8, 8);
    pmr_deallocate(&missing_vtable, &stack_object, 8, 8);
    pmr_deallocate(&missing_function, &stack_object, 8, 8);
    pmr_deallocate(pmr_new_delete_resource(), NULL, 8, 8);
    pmr_deallocate(pmr_new_delete_resource(), &stack_object, 8, 0);
    pmr_deallocate(pmr_new_delete_resource(), &stack_object, 8, 3);
    pmr_allocator_deallocate(empty, &stack_object, 8, 8);
}

TEST_CASE(test_pmr_zero_size, "[pmr]") {
    CountingResource counting;
    counting_init(&counting, pmr_new_delete_resource());
    int stack_object = 0;

    REQUIRE(pmr_allocate(&counting.base, 0, 8) == NULL);
    REQUIRE(counting.allocate_calls == 0);
    REQUIRE(counting.live_blocks == 0);

    pmr_deallocate(&counting.base, NULL, 0, 8);
    pmr_deallocate(&counting.base, NULL, 4, 8);
    pmr_deallocate(&counting.base, &stack_object, 0, 8);
    REQUIRE(counting.deallocate_calls == 0);
    REQUIRE(counting.live_blocks == 0);
}

TEST_CASE(test_pmr_new_delete_round_trip, "[pmr]") {
    static const size_t sizes[] = {1u, 2u, 3u, 8u, 24u, 4096u};
    const size_t alignments[] = {
        1u,
        _Alignof(max_align_t),
        2u * _Alignof(max_align_t),
        64u,
        256u
    };

    PmrResource *resource = pmr_new_delete_resource();
    REQUIRE(resource == pmr_new_delete_resource());
    /* 1 and alignof(max_align_t) take malloc. 64 and 256 take the aligned path. */
    REQUIRE(_Alignof(max_align_t) < 64u);

    for (size_t size_index = 0;
        size_index < sizeof(sizes) / sizeof(sizes[0]); ++size_index) {
        for (size_t align_index = 0;
            align_index < sizeof(alignments) / sizeof(alignments[0]); ++align_index) {
            const size_t bytes = sizes[size_index];
            const size_t alignment = alignments[align_index];
            unsigned char *block = pmr_allocate(resource, bytes, alignment);
            REQUIRE(block != NULL);
            REQUIRE(((uintptr_t)block % alignment) == 0u);
            memset(block, 0xA5, bytes);
            REQUIRE(block[0] == 0xA5);
            REQUIRE(block[bytes - 1u] == 0xA5);
            pmr_deallocate(resource, block, bytes, alignment);
        }
    }

    const PmrAllocator allocator = pmr_allocator_from(resource);
    void *block = pmr_allocator_allocate(allocator, 24, 64);
    REQUIRE(block != NULL);
    REQUIRE(((uintptr_t)block % 64u) == 0u);
    memset(block, 0x5A, 24);
    pmr_allocator_deallocate(allocator, block, 24, 64);
}

TEST_CASE(test_pmr_null_resource, "[pmr]") {
    const PmrResource *resource = pmr_null_memory_resource();
    REQUIRE(resource == pmr_null_memory_resource());
    REQUIRE(resource != pmr_new_delete_resource());
    REQUIRE(pmr_allocate(resource, 32, 8) == NULL);
    REQUIRE(pmr_allocate(resource, 1, _Alignof(max_align_t)) == NULL);
    pmr_deallocate(resource, NULL, 0, 8);
    pmr_deallocate(resource, NULL, 4, 8);
}

TEST_CASE(test_pmr_equality, "[pmr]") {
    CountingResource left;
    CountingResource right;
    const PmrResource never = {&never_equal_vtable};
    const PmrResource always = {&always_equal_vtable};
    const PmrAllocator new_delete_a = pmr_allocator_from(pmr_new_delete_resource());
    const PmrAllocator new_delete_b = pmr_allocator_from(pmr_new_delete_resource());
    const PmrAllocator empty_a = pmr_allocator_from(NULL);
    const PmrAllocator empty_b = {0};

    counting_init(&left, pmr_new_delete_resource());
    counting_init(&right, pmr_new_delete_resource());

    REQUIRE(pmr_is_equal(pmr_new_delete_resource(), pmr_new_delete_resource()) != 0);
    REQUIRE(pmr_is_equal(&left.base, &left.base) != 0);
    REQUIRE(pmr_is_equal(pmr_new_delete_resource(), pmr_null_memory_resource()) == 0);
    REQUIRE(pmr_is_equal(&left.base, &right.base) == 0);
    REQUIRE(pmr_allocator_is_equal(new_delete_a, new_delete_b) != 0);
    REQUIRE(pmr_allocator_is_equal(pmr_allocator_from(&left.base),
                                   pmr_allocator_from(&right.base)) == 0);
    REQUIRE(pmr_allocator_is_equal(empty_a, empty_b) != 0);
    REQUIRE(pmr_allocator_is_equal(empty_a, new_delete_a) == 0);
    REQUIRE(pmr_is_equal(NULL, NULL) == 0);

    REQUIRE(pmr_is_same_object(pmr_new_delete_resource(), pmr_new_delete_resource()) != 0);
    REQUIRE(pmr_is_same_object(pmr_new_delete_resource(), pmr_null_memory_resource()) == 0);

    /* Address equality wins before the callback. Different objects use one side only. */
    REQUIRE(pmr_is_equal(&never, &never) != 0);
    REQUIRE(pmr_is_equal(&never, pmr_new_delete_resource()) == 0);
    REQUIRE(pmr_is_equal(&always, pmr_new_delete_resource()) != 0);
    REQUIRE(pmr_is_equal(pmr_new_delete_resource(), &always) == 0);
}

TEST_CASE(test_pmr_select_on_copy, "[pmr]") {
    const PmrAllocator source = pmr_allocator_from(pmr_null_memory_resource());

    pmr_set_default_resource(NULL);
    PmrAllocator copied = pmr_allocator_select_on_copy(source);
    REQUIRE(pmr_allocator_resource(copied) == pmr_new_delete_resource());
    REQUIRE(pmr_allocator_resource(source) == pmr_null_memory_resource());

    const PmrResource *previous = pmr_set_default_resource(pmr_null_memory_resource());
    REQUIRE(previous == pmr_new_delete_resource());
    copied = pmr_allocator_select_on_copy(source);
    REQUIRE(pmr_allocator_resource(copied) == pmr_null_memory_resource());
    REQUIRE(pmr_allocator_resource(source) == pmr_null_memory_resource());

    previous = pmr_set_default_resource(NULL);
    REQUIRE(previous == pmr_null_memory_resource());
    REQUIRE(pmr_get_default_resource() == pmr_new_delete_resource());
}

TEST_CASE(test_pmr_counting_resource, "[pmr]") {
    CountingResource counting;
    PmrResource failing = {&always_null_vtable};

    counting_init(&counting, pmr_new_delete_resource());
    const PmrAllocator allocator = pmr_allocator_from(&counting.base);

    unsigned char *block = pmr_allocator_allocate(allocator, 32, 8);
    REQUIRE(block != NULL);
    REQUIRE(((uintptr_t)block % 8u) == 0u);
    memset(block, 0x11, 32);
    REQUIRE(counting.live_blocks == 1);
    REQUIRE(counting.live_bytes == 32);
    REQUIRE(counting.allocate_calls == 1);

    pmr_allocator_deallocate(allocator, block, 32, 8);
    REQUIRE(counting.live_blocks == 0);
    REQUIRE(counting.live_bytes == 0);
    REQUIRE(counting.deallocate_calls == 1);

    counting_init(&counting, &failing);
    REQUIRE(pmr_allocate(&counting.base, 32, 8) == NULL);
    REQUIRE(counting.allocate_calls == 1);
    REQUIRE(counting.live_blocks == 0);
    REQUIRE(counting.live_bytes == 0);
}

TEST_CASE(test_pmr_allocate_array_overflow, "[pmr]") {
    CountingResource counting;
    const size_t overflowing = SIZE_MAX / 2u + 1u;

    counting_init(&counting, pmr_new_delete_resource());
    const PmrAllocator allocator = pmr_allocator_from(&counting.base);

    REQUIRE(pmr_allocator_allocate_array(allocator, overflowing, 2, _Alignof(max_align_t)) == NULL);
    REQUIRE(counting.allocate_calls == 0);
    REQUIRE(counting.live_blocks == 0);
    REQUIRE(counting.live_bytes == 0);

    REQUIRE(pmr_allocator_allocate_array(allocator, 0, sizeof(int), _Alignof(int)) == NULL);
    REQUIRE(counting.allocate_calls == 0);

    void *block = pmr_allocator_allocate_array(allocator, 4, sizeof(int), _Alignof(int));
    REQUIRE(block != NULL);
    REQUIRE(counting.live_blocks == 1);
    REQUIRE(counting.live_bytes == 4u * sizeof(int));
    memset(block, 0x22, 4u * sizeof(int));

    pmr_allocator_deallocate_array(allocator, block, overflowing, 2, _Alignof(int));
    REQUIRE(counting.deallocate_calls == 0);
    REQUIRE(counting.live_blocks == 1);

    pmr_allocator_deallocate_array(allocator, block, 4, sizeof(int), _Alignof(int));
    REQUIRE(counting.deallocate_calls == 1);
    REQUIRE(counting.live_blocks == 0);
    REQUIRE(counting.live_bytes == 0);
}

TEST_CASE(test_pmr_default_resource, "[pmr]") {
    PmrResource *custom = pmr_null_memory_resource();

    REQUIRE(pmr_get_default_resource() == pmr_new_delete_resource());

    const PmrResource *previous = pmr_set_default_resource(custom);
    REQUIRE(previous == pmr_new_delete_resource());
    REQUIRE(pmr_get_default_resource() == custom);

    PmrAllocator snapshot = pmr_allocator_default();
    REQUIRE(pmr_allocator_resource(snapshot) == custom);

    previous = pmr_set_default_resource(NULL);
    REQUIRE(previous == custom);
    REQUIRE(pmr_get_default_resource() == pmr_new_delete_resource());
    REQUIRE(pmr_allocator_resource(snapshot) == custom);

    snapshot = pmr_allocator_default();
    REQUIRE(pmr_allocator_resource(snapshot) == pmr_new_delete_resource());
}
