// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#include <bedrock/pinned_colony.h>
#include <bedrock/unibench.h>

#include <stdint.h>

BENCHMARK(bench_pinned_colony_insert_iterate_erase, "[pinned_colony]") {
    enum { N = 4096 };
    PinnedColony *colony = pinned_colony_create((size_t)N, sizeof(int), _Alignof(int), NULL, NULL);
    if (!colony) {
        return;
    }

    for (int value = 0; value < N; ++value) {
        if (!pinned_colony_insert(colony, &value)) {
            pinned_colony_destroy(colony);
            return;
        }
    }

    size_t sink = 0;
    for (size_t index = pinned_colony_next_index(colony, SIZE_MAX);
         index != SIZE_MAX;
         index = pinned_colony_next_index(colony, index)) {
        const int *element = pinned_colony_at(colony, index);
        sink += element ? (size_t)(*element) : 0u;
    }

    size_t index = pinned_colony_next_index(colony, SIZE_MAX);
    int drop = 0;
    while (index != SIZE_MAX) {
        const size_t next = pinned_colony_next_index(colony, index);
        if (drop) {
            pinned_colony_erase(colony, pinned_colony_at(colony, index));
        }
        drop = !drop;
        index = next;
    }

    for (index = pinned_colony_next_index(colony, SIZE_MAX);
         index != SIZE_MAX;
         index = pinned_colony_next_index(colony, index)) {
        const int *element = pinned_colony_at(colony, index);
        sink += element ? (size_t)(*element) : 0u;
    }

    (void)sink;
    pinned_colony_destroy(colony);
}

BENCHMARK(bench_pinned_colony_acquire_commit, "[pinned_colony]") {
    enum { N = 4096, BYTES = 4096 };
    PinnedColony *colony = pinned_colony_create((size_t)N, (size_t)BYTES, 64u, NULL, NULL);
    if (!colony) {
        return;
    }

    for (int i = 0; i < N; ++i) {
        unsigned char *slot = pinned_colony_acquire(colony);
        if (!slot) {
            pinned_colony_destroy(colony);
            return;
        }
        slot[0] = 1;
        slot[BYTES - 1] = 1;
    }

    pinned_colony_destroy(colony);
}

BENCHMARK(bench_pinned_colony_reacquire_committed, "[pinned_colony]") {
    enum { N = 4096, ROUNDS = 100 };
    PinnedColony *colony = pinned_colony_create((size_t)N, 64u, 64u, NULL, NULL);
    if (!colony) {
        return;
    }

    for (int i = 0; i < N; ++i) {
        unsigned char *slot = pinned_colony_acquire(colony);
        if (!slot) {
            pinned_colony_destroy(colony);
            return;
        }
        slot[0] = 1;
        slot[63] = 1;
    }

    for (int round = 0; round < ROUNDS; ++round) {
        size_t index = pinned_colony_next_index(colony, SIZE_MAX);
        int drop = 1;
        while (index != SIZE_MAX) {
            const size_t next = pinned_colony_next_index(colony, index);
            if (drop) {
                pinned_colony_erase(colony, pinned_colony_at(colony, index));
            }
            drop = !drop;
            index = next;
        }
        for (;;) {
            unsigned char *slot = pinned_colony_acquire(colony);
            if (!slot) {
                break;
            }
            slot[0] = 1;
        }
    }

    pinned_colony_destroy(colony);
}

static size_t walk_live(PinnedColony *colony) {
    size_t sum = 0;
    for (size_t index = pinned_colony_next_index(colony, SIZE_MAX);
         index != SIZE_MAX;
         index = pinned_colony_next_index(colony, index)) {
        const int *element = pinned_colony_at(colony, index);
        sum += element ? (size_t)(*element) : 0u;
    }
    return sum;
}

BENCHMARK(bench_pinned_colony_iterate_alternating, "[pinned_colony]") {
    enum { N = 1 << 20, PASSES = 20 };
    PinnedColony *colony = pinned_colony_create((size_t)N, sizeof(int), _Alignof(int), NULL, NULL);
    if (!colony) {
        return;
    }

    for (int value = 0; value < N; ++value) {
        if (!pinned_colony_insert(colony, &value)) {
            pinned_colony_destroy(colony);
            return;
        }
    }

    size_t index = pinned_colony_next_index(colony, SIZE_MAX);
    int drop = 1;
    while (index != SIZE_MAX) {
        const size_t next = pinned_colony_next_index(colony, index);
        if (drop) {
            pinned_colony_erase(colony, pinned_colony_at(colony, index));
        }
        drop = !drop;
        index = next;
    }

    size_t sink = 0;
    for (int pass = 0; pass < PASSES; ++pass) {
        sink += walk_live(colony);
    }
    (void)sink;
    pinned_colony_destroy(colony);
}

BENCHMARK(bench_pinned_colony_erase_contiguous, "[pinned_colony]") {
    enum { N = 16000 };
    PinnedColony *colony = pinned_colony_create((size_t)N, sizeof(int), _Alignof(int), NULL, NULL);
    if (!colony) {
        return;
    }

    for (int value = 0; value < N; ++value) {
        if (!pinned_colony_insert(colony, &value)) {
            pinned_colony_destroy(colony);
            return;
        }
    }

    while (!pinned_colony_empty(colony)) {
        const size_t index = pinned_colony_next_index(colony, SIZE_MAX);
        pinned_colony_erase(colony, pinned_colony_at(colony, index));
    }

    pinned_colony_destroy(colony);
}

static void count_dtor(void *element, void *user) {
    (void)element;
    size_t *count = user;
    *count += 1u;
}

BENCHMARK(bench_pinned_colony_clear_dtor, "[pinned_colony]") {
    enum { N = 20000 };
    size_t destroyed = 0;
    PinnedColony *colony = pinned_colony_create((size_t)N, sizeof(int), _Alignof(int),
                                                count_dtor, &destroyed);
    if (!colony) {
        return;
    }

    for (int value = 0; value < N; ++value) {
        if (!pinned_colony_insert(colony, &value)) {
            pinned_colony_destroy(colony);
            return;
        }
    }

    pinned_colony_clear(colony);
    (void)destroyed;
    pinned_colony_destroy(colony);
}
