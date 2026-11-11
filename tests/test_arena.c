// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#include <bedrock/arena_allocator.h>
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
