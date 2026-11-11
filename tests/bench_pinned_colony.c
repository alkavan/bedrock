// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#include <bedrock/pinned_colony.h>
#include <bedrock/unitest.h>

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
