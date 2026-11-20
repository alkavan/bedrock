// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#include <bedrock/arena.h>
#include <bedrock/unitest.h>

#include <stdint.h>
#include <string.h>

TEST_CASE(test_arena_create_and_queries, "[arena]") {
    REQUIRE(arena_create(0) == NULL);
    REQUIRE(arena_capacity(NULL) == 0);
    REQUIRE(arena_used(NULL) == 0);
    REQUIRE(arena_alloc(NULL, 8) == NULL);
    arena_free(NULL, NULL);
    arena_destroy(NULL);

    MemHeap *heap = arena_create(100);
    REQUIRE(heap != NULL);
    REQUIRE(arena_capacity(heap) == 96);
    REQUIRE(arena_used(heap) == 0);
    REQUIRE(arena_alloc(heap, 0) == NULL);
    REQUIRE(arena_alloc(heap, 80) != NULL);
    REQUIRE(arena_used(heap) == 96);
    REQUIRE(arena_alloc(heap, 1) == NULL);
    REQUIRE(arena_used(heap) == 96);
    arena_destroy(heap);
}

TEST_CASE(test_arena_alloc_free_and_reuse, "[arena]") {
    MemHeap *heap = arena_create(256);

    REQUIRE(heap != NULL);
    void *first = arena_alloc(heap, 1);
    REQUIRE(first != NULL);
    REQUIRE(((uintptr_t)first % 16u) == 0u);
    memset(first, 0xAB, 1);
    REQUIRE(arena_used(heap) == 32);

    void *second = arena_alloc(heap, 1);
    REQUIRE(second != NULL);
    REQUIRE(second != first);
    REQUIRE(arena_used(heap) == 64);

    arena_free(heap, NULL);
    arena_free(heap, first);
    REQUIRE(arena_used(heap) == 32);
    void *again = arena_alloc(heap, 1);
    REQUIRE(again == first);
    REQUIRE(arena_used(heap) == 64);

    arena_free(heap, again);
    arena_free(heap, second);
    REQUIRE(arena_used(heap) == 0);

    /* The two holes must coalesce, or this 48-byte payload cannot fit. */
    again = arena_alloc(heap, 48);
    REQUIRE(again != NULL);
    REQUIRE(arena_used(heap) == 64);
    memset(again, 0xCD, 48);

    arena_free(heap, again);
    REQUIRE(arena_used(heap) == 0);
    arena_destroy(heap);
}

static size_t payload_span(const size_t n) {
    return (n + 16u + 15u) & ~(size_t)15u;
}

TEST_CASE(test_arena_reset_discards_without_unmap, "[arena]") {
    arena_reset(NULL);

    MemHeap *heap = arena_create(4096);
    REQUIRE(heap != NULL);

    void *first = arena_alloc(heap, 32);
    void *second = arena_alloc(heap, 1000);
    REQUIRE(first != NULL);
    REQUIRE(second != NULL);
    memset(first, 0x11, 32);
    REQUIRE(arena_used(heap) > 0);

    for (int i = 0; i < 50; ++i) {
        void *block = arena_alloc(heap, 24);
        REQUIRE(block != NULL);
        if ((i % 2) == 0) {
            arena_free(heap, block);
        }
    }

    arena_reset(heap);
    REQUIRE(arena_used(heap) == 0);
    REQUIRE(arena_capacity(heap) == 4096u);

    void *whole = arena_alloc(heap, 4080);
    REQUIRE(whole != NULL);
    REQUIRE(arena_used(heap) == 4096u);
    memset(whole, 0x5A, 4080);
    arena_reset(heap);
    REQUIRE(arena_used(heap) == 0);
    arena_destroy(heap);
}

TEST_CASE(test_arena_alloc_aligned, "[arena]") {
    MemHeap *heap = arena_create(8192);
    REQUIRE(heap != NULL);
    REQUIRE(arena_alloc_aligned(NULL, 8, 32) == NULL);
    REQUIRE(arena_alloc_aligned(heap, 0, 32) == NULL);
    REQUIRE(arena_alloc_aligned(heap, 8, 0) == NULL);
    REQUIRE(arena_alloc_aligned(heap, 8, 3) == NULL);
    REQUIRE(arena_alloc_aligned(heap, 8, 24) == NULL);
    REQUIRE(arena_alloc(heap, SIZE_MAX) == NULL);
    REQUIRE(arena_used(heap) == 0);

    unsigned char *wide = arena_alloc_aligned(heap, 32, 64);
    unsigned char *next = arena_alloc_aligned(heap, 32, 64);
    REQUIRE(wide != NULL);
    REQUIRE(next != NULL);
    REQUIRE(((uintptr_t)wide % 64u) == 0u);
    REQUIRE(((uintptr_t)next % 64u) == 0u);
    REQUIRE(next != wide);
    memset(wide, 0x11, 32);
    memset(next, 0x22, 32);

    unsigned char *avx = arena_alloc_aligned(heap, 64, 32);
    REQUIRE(avx != NULL);
    REQUIRE(((uintptr_t)avx % 32u) == 0u);
    memset(avx, 0x33, 64);

    arena_free(heap, wide);
    REQUIRE(next[0] == 0x22 && next[31] == 0x22);
    REQUIRE(avx[0] == 0x33 && avx[63] == 0x33);
    arena_free(heap, next);
    arena_free(heap, avx);
    REQUIRE(arena_used(heap) == 0);

    unsigned char *again = arena_alloc_aligned(heap, 128, 64);
    REQUIRE(again != NULL);
    REQUIRE(((uintptr_t)again % 64u) == 0u);
    arena_free(heap, again);
    REQUIRE(arena_used(heap) == 0);

    void *small = arena_alloc(heap, 1);
    REQUIRE(small != NULL);
    REQUIRE(((uintptr_t)small % 16u) == 0u);
    REQUIRE(arena_used(heap) == payload_span(1));
    arena_free(heap, small);
    REQUIRE(arena_used(heap) == 0);
    arena_destroy(heap);
}
