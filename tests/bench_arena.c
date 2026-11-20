// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#include <bedrock/arena.h>
#include <bedrock/unibench.h>

#include <stdint.h>
#include <stdlib.h>

static void touch(void *p, const size_t n) {
    if (!p || n == 0) {
        return;
    }
    ((unsigned char *)p)[0] = 1;
    ((unsigned char *)p)[n - 1] = 1;
}

BENCHMARK(bench_arena_lifo_free, "[arena]") {
    enum { N = 100000, BYTES = 256 };
    void **ptrs = calloc((size_t)N, sizeof(void *));
    MemHeap *heap = arena_create(((size_t)N * (BYTES + 32u)) + 4096u);
    if (!ptrs || !heap) {
        arena_destroy(heap);
        free(ptrs);
        return;
    }

    for (size_t i = 0; i < (size_t)N; ++i) {
        ptrs[i] = arena_alloc(heap, BYTES);
        if (!ptrs[i]) {
            arena_destroy(heap);
            free(ptrs);
            return;
        }
        touch(ptrs[i], BYTES);
    }
    for (size_t i = (size_t)N; i-- > 0;) {
        arena_free(heap, ptrs[i]);
    }

    arena_destroy(heap);
    free(ptrs);
}

BENCHMARK(bench_arena_reset, "[arena]") {
    enum { RESETS = 5000000 };
    MemHeap *heap = arena_create(1u << 20);
    if (!heap) {
        return;
    }

    void *block = arena_alloc(heap, 4096u);
    if (block) {
        touch(block, 4096u);
    }
    arena_reset(heap);
    for (int i = 0; i < RESETS; ++i) {
        arena_reset(heap);
    }

    arena_destroy(heap);
}

BENCHMARK(bench_arena_destroy_recreate, "[arena]") {
    enum { N = 100000, BYTES = 256 };
    const size_t cap = ((size_t)N * (BYTES + 32u)) + 4096u;
    void **ptrs = calloc((size_t)N, sizeof(void *));
    MemHeap *heap = arena_create(cap);
    if (!ptrs || !heap) {
        arena_destroy(heap);
        free(ptrs);
        return;
    }

    for (size_t i = 0; i < (size_t)N; ++i) {
        ptrs[i] = arena_alloc(heap, BYTES);
        if (!ptrs[i]) {
            arena_destroy(heap);
            free(ptrs);
            return;
        }
        touch(ptrs[i], BYTES);
    }

    arena_destroy(heap);
    heap = arena_create(cap);
    if (!heap) {
        free(ptrs);
        return;
    }
    for (size_t i = 0; i < (size_t)N; ++i) {
        ptrs[i] = arena_alloc(heap, BYTES);
        if (!ptrs[i]) {
            break;
        }
        touch(ptrs[i], BYTES);
    }

    arena_destroy(heap);
    free(ptrs);
}

BENCHMARK(bench_arena_checkerboard_free, "[arena]") {
    enum { N = 40000, BYTES = 64 };
    void **ptrs = calloc((size_t)N, sizeof(void *));
    MemHeap *heap = arena_create(((size_t)N * (BYTES + 32u)) + 4096u);
    if (!ptrs || !heap) {
        arena_destroy(heap);
        free(ptrs);
        return;
    }

    for (size_t i = 0; i < (size_t)N; ++i) {
        ptrs[i] = arena_alloc(heap, BYTES);
        if (!ptrs[i]) {
            arena_destroy(heap);
            free(ptrs);
            return;
        }
    }
    for (size_t i = 0; i < (size_t)N; i += 2u) {
        arena_free(heap, ptrs[i]);
    }
    for (size_t i = 1; i < (size_t)N; i += 2u) {
        arena_free(heap, ptrs[i]);
    }

    arena_destroy(heap);
    free(ptrs);
}
