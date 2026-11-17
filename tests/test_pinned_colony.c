// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#include <bedrock/unitest.h>
#include <bedrock/pinned_colony.h>
#include <bedrock/vmem.h>

#include <stdint.h>
#include <string.h>

static void count_dtor(void *element, void *user) {
    (void)element;
    size_t *count = user;
    ++(*count);
}

static size_t collect_indices(PinnedColony *colony, size_t *out, const size_t cap) {
    size_t count = 0;
    for (size_t index = pinned_colony_next_index(colony, SIZE_MAX);
         index != SIZE_MAX;
         index = pinned_colony_next_index(colony, index)) {
        REQUIRE(pinned_colony_at(colony, index) != NULL);
        if (count < cap) {
            out[count] = index;
        }
        ++count;
    }
    return count;
}

TEST_CASE(test_pinned_colony_create_destroy, "[pinned_colony]") {
    PinnedColony *colony = pinned_colony_create(8, sizeof(int), _Alignof(int), NULL, NULL);
    REQUIRE(colony != NULL);
    REQUIRE(pinned_colony_size(colony) == 0u);
    REQUIRE(pinned_colony_capacity(colony) == 8u);
    REQUIRE(pinned_colony_empty(colony));
    REQUIRE(pinned_colony_next_index(colony, SIZE_MAX) == SIZE_MAX);
    REQUIRE(pinned_colony_prev_index(colony, SIZE_MAX) == SIZE_MAX);
    pinned_colony_destroy(colony);

    pinned_colony_destroy(NULL);
    pinned_colony_clear(NULL);
    REQUIRE(pinned_colony_size(NULL) == 0u);
    REQUIRE(pinned_colony_capacity(NULL) == 0u);
    REQUIRE(pinned_colony_empty(NULL));
    REQUIRE(pinned_colony_acquire(NULL) == NULL);
    REQUIRE(pinned_colony_insert(NULL, &(int){1}) == NULL);
    REQUIRE(pinned_colony_erase(NULL, NULL) == -1);
}

TEST_CASE(test_pinned_colony_reject_bad_args, "[pinned_colony]") {
    REQUIRE(pinned_colony_create(4, 0, 1, NULL, NULL) == NULL);
    REQUIRE(pinned_colony_create(4, sizeof(int), 3, NULL, NULL) == NULL);
    REQUIRE(pinned_colony_create(4, sizeof(int), vmem_page_size() * 2u, NULL, NULL) == NULL);
    REQUIRE(pinned_colony_create(SIZE_MAX, 1, 1, NULL, NULL) == NULL);

    PinnedColony *empty = pinned_colony_create(0, sizeof(int), 0, NULL, NULL);
    REQUIRE(empty != NULL);
    REQUIRE(pinned_colony_capacity(empty) == 0u);
    REQUIRE(pinned_colony_acquire(empty) == NULL);
    REQUIRE(pinned_colony_insert(empty, &(int){7}) == NULL);
    pinned_colony_destroy(empty);
}

TEST_CASE(test_pinned_colony_insert_and_iterate, "[pinned_colony]") {
    PinnedColony *colony = pinned_colony_create(8, sizeof(int), _Alignof(int), NULL, NULL);
    REQUIRE(colony != NULL);

    int *slots[5];
    for (int value = 0; value < 5; ++value) {
        slots[value] = pinned_colony_insert(colony, &value);
        REQUIRE(slots[value] != NULL);
        REQUIRE(*slots[value] == value);
    }
    REQUIRE(pinned_colony_insert(colony, NULL) == NULL);
    REQUIRE(pinned_colony_size(colony) == 5u);
    REQUIRE(!pinned_colony_empty(colony));

    size_t indices[8];
    const size_t count = collect_indices(colony, indices, 8);
    REQUIRE(count == 5u);
    for (size_t i = 0; i < count; ++i) {
        REQUIRE(indices[i] == i);
        const int *element = pinned_colony_at(colony, indices[i]);
        REQUIRE(element == slots[i]);
        REQUIRE(*element == (int)i);
        REQUIRE(pinned_colony_index(colony, element) == i);
    }

    size_t reversed[8];
    size_t rev = 0;
    for (size_t index = pinned_colony_prev_index(colony, SIZE_MAX);
         index != SIZE_MAX;
         index = pinned_colony_prev_index(colony, index)) {
        reversed[rev++] = index;
    }
    REQUIRE(rev == 5u);
    REQUIRE(reversed[0] == 4u);
    REQUIRE(reversed[4] == 0u);

    int source = 42;
    const int *copied = pinned_colony_insert(colony, &source);
    REQUIRE(copied != NULL);
    source = 0;
    REQUIRE(*copied == 42);

    pinned_colony_destroy(colony);
}

TEST_CASE(test_pinned_colony_pointer_stability, "[pinned_colony]") {
    enum { N = 100 };
    PinnedColony *colony = pinned_colony_create(N, sizeof(int), _Alignof(int), NULL, NULL);
    REQUIRE(colony != NULL);

    int *slots[N];
    for (int value = 0; value < N; ++value) {
        slots[value] = pinned_colony_insert(colony, &value);
        REQUIRE(slots[value] != NULL);
    }

    for (int value = 0; value < N; ++value) {
        REQUIRE(slots[value] != NULL);
        REQUIRE(*slots[value] == value);
        REQUIRE(pinned_colony_index(colony, slots[value]) == (size_t)value);
    }

    pinned_colony_destroy(colony);
}

TEST_CASE(test_pinned_colony_hole_reuse, "[pinned_colony]") {
    PinnedColony *colony = pinned_colony_create(8, sizeof(int), _Alignof(int), NULL, NULL);
    REQUIRE(colony != NULL);

    int *slots[5];
    for (int value = 0; value < 5; ++value) {
        slots[value] = pinned_colony_insert(colony, &value);
        REQUIRE(slots[value] != NULL);
    }

    REQUIRE(pinned_colony_erase(colony, slots[2]) == 0);
    REQUIRE(pinned_colony_erase(colony, slots[3]) == 0);
    REQUIRE(pinned_colony_erase(colony, slots[1]) == 0);
    REQUIRE(pinned_colony_size(colony) == 2u);

    size_t indices[8];
    REQUIRE(collect_indices(colony, indices, 8) == 2u);
    REQUIRE(indices[0] == 0u);
    REQUIRE(indices[1] == 4u);
    REQUIRE(*slots[0] == 0);
    REQUIRE(*slots[4] == 4);

    const int reused_values[] = {100, 101, 102};
    const int *first = pinned_colony_insert(colony, &reused_values[0]);
    const int *second = pinned_colony_insert(colony, &reused_values[1]);
    const int *third = pinned_colony_insert(colony, &reused_values[2]);
    REQUIRE(first == slots[1]);
    REQUIRE(second == slots[3]);
    REQUIRE(third == slots[2]);
    REQUIRE(*slots[0] == 0);
    REQUIRE(*slots[4] == 4);

    REQUIRE(collect_indices(colony, indices, 8) == 5u);
    for (size_t i = 0; i < 5u; ++i) {
        const size_t expected[] = {0u, 1u, 2u, 3u, 4u};
        REQUIRE(indices[i] == expected[i]);
    }

    pinned_colony_destroy(colony);
}

TEST_CASE(test_pinned_colony_skipfield_merge_and_split, "[pinned_colony]") {
    PinnedColony *colony = pinned_colony_create(8, sizeof(int), _Alignof(int), NULL, NULL);
    REQUIRE(colony != NULL);

    int *slots[7];
    for (int value = 0; value < 7; ++value) {
        slots[value] = pinned_colony_insert(colony, &value);
        REQUIRE(slots[value] != NULL);
    }

    REQUIRE(pinned_colony_erase(colony, slots[1]) == 0);
    REQUIRE(pinned_colony_erase(colony, slots[2]) == 0);
    REQUIRE(pinned_colony_erase(colony, slots[4]) == 0);
    REQUIRE(pinned_colony_erase(colony, slots[5]) == 0);
    REQUIRE(pinned_colony_erase(colony, slots[3]) == 0);

    size_t indices[8];
    REQUIRE(collect_indices(colony, indices, 8) == 2u);
    REQUIRE(indices[0] == 0u);
    REQUIRE(indices[1] == 6u);
    REQUIRE(*slots[0] == 0);
    REQUIRE(*slots[6] == 6);

    for (size_t step = 0; step < 5u; ++step) {
        const size_t revive[] = {3u, 5u, 4u, 2u, 1u};
        const int value = 50 + (int)step;
        const int *slot = pinned_colony_insert(colony, &value);
        REQUIRE(slot == slots[revive[step]]);
        REQUIRE(*slot == value);
    }

    REQUIRE(collect_indices(colony, indices, 8) == 7u);
    for (size_t i = 0; i < 7u; ++i) {
        REQUIRE(indices[i] == i);
    }

    size_t reversed = pinned_colony_prev_index(colony, SIZE_MAX);
    for (size_t expect = 6; expect != SIZE_MAX; --expect) {
        REQUIRE(reversed == expect);
        reversed = pinned_colony_prev_index(colony, reversed);
        if (expect == 0) {
            break;
        }
    }

    REQUIRE(reversed == SIZE_MAX);
    pinned_colony_destroy(colony);
}

TEST_CASE(test_pinned_colony_capacity, "[pinned_colony]") {
    PinnedColony *colony = pinned_colony_create(3, sizeof(int), _Alignof(int), NULL, NULL);
    REQUIRE(colony != NULL);

    const int values[] = {1, 2, 3, 4};
    REQUIRE(pinned_colony_insert(colony, &values[0]) != NULL);
    REQUIRE(pinned_colony_insert(colony, &values[1]) != NULL);

    int *third = pinned_colony_insert(colony, &values[2]);
    REQUIRE(third != NULL);
    REQUIRE(pinned_colony_insert(colony, &values[3]) == NULL);
    REQUIRE(pinned_colony_acquire(colony) == NULL);
    REQUIRE(pinned_colony_size(colony) == 3u);
    REQUIRE(pinned_colony_erase(colony, third) == 0);
    REQUIRE(pinned_colony_size(colony) == 2u);

    const int *reused = pinned_colony_insert(colony, &values[3]);
    REQUIRE(reused == third);
    REQUIRE(*reused == 4);
    REQUIRE(pinned_colony_size(colony) == 3u);
    REQUIRE(pinned_colony_insert(colony, &values[0]) == NULL);

    pinned_colony_destroy(colony);
}

TEST_CASE(test_pinned_colony_clear, "[pinned_colony]") {
    size_t destroyed = 0;
    PinnedColony *colony = pinned_colony_create(4, sizeof(int), _Alignof(int), count_dtor, &destroyed);
    REQUIRE(colony != NULL);

    int *slots[3];
    for (int value = 0; value < 3; ++value) {
        slots[value] = pinned_colony_insert(colony, &value);
        REQUIRE(slots[value] != NULL);
    }

    REQUIRE(pinned_colony_erase(colony, slots[0]) == 0);
    REQUIRE(destroyed == 1u);

    pinned_colony_clear(colony);
    REQUIRE(destroyed == 3u);
    REQUIRE(pinned_colony_size(colony) == 0u);
    REQUIRE(pinned_colony_empty(colony));
    REQUIRE(pinned_colony_next_index(colony, SIZE_MAX) == SIZE_MAX);

    const int again = 9;
    const int *revived = pinned_colony_insert(colony, &again);
    REQUIRE(revived == slots[2]);
    REQUIRE(*revived == 9);
    REQUIRE(pinned_colony_size(colony) == 1u);

    pinned_colony_destroy(colony);
    REQUIRE(destroyed == 4u);
}

TEST_CASE(test_pinned_colony_destructor, "[pinned_colony]") {
    size_t destroyed = 0;
    PinnedColony *colony = pinned_colony_create(4, sizeof(int), 0, count_dtor, &destroyed);
    REQUIRE(colony != NULL);

    const int values[] = {1, 2, 3};
    const int *first = pinned_colony_insert(colony, &values[0]);
    int *second = pinned_colony_insert(colony, &values[1]);
    const int *third = pinned_colony_insert(colony, &values[2]);
    REQUIRE(first != NULL);
    REQUIRE(second != NULL);
    REQUIRE(third != NULL);

    REQUIRE(pinned_colony_erase(colony, second) == 0);
    REQUIRE(destroyed == 1u);
    REQUIRE(pinned_colony_erase(colony, second) == -1);
    REQUIRE(destroyed == 1u);
    REQUIRE(pinned_colony_index(colony, second) == SIZE_MAX);

    pinned_colony_destroy(colony);
    REQUIRE(destroyed == 3u);
}

TEST_CASE(test_pinned_colony_reject_bad_pointer, "[pinned_colony]") {
    PinnedColony *colony = pinned_colony_create(4, sizeof(int), _Alignof(int), NULL, NULL);
    REQUIRE(colony != NULL);

    const int value = 5;
    int *slot = pinned_colony_insert(colony, &value);
    REQUIRE(slot != NULL);

    int outside = 5;
    REQUIRE(pinned_colony_index(colony, NULL) == SIZE_MAX);
    REQUIRE(pinned_colony_index(colony, &outside) == SIZE_MAX);
    REQUIRE(pinned_colony_index(colony, (unsigned char *)slot + 1) == SIZE_MAX);
    REQUIRE(pinned_colony_erase(colony, NULL) == -1);
    REQUIRE(pinned_colony_erase(colony, &outside) == -1);
    REQUIRE(pinned_colony_erase(colony, (unsigned char *)slot + 1) == -1);
    REQUIRE(pinned_colony_at(colony, 3) == NULL);
    REQUIRE(pinned_colony_at(colony, SIZE_MAX) == NULL);

    REQUIRE(pinned_colony_erase(colony, slot) == 0);
    REQUIRE(pinned_colony_index(colony, slot) == SIZE_MAX);
    REQUIRE(pinned_colony_at(colony, 0) == NULL);
    REQUIRE(pinned_colony_erase(colony, slot) == -1);

    pinned_colony_destroy(colony);
}

TEST_CASE(test_pinned_colony_stride, "[pinned_colony]") {
    PinnedColony *colony = pinned_colony_create(4, 3, 4, NULL, NULL);
    REQUIRE(colony != NULL);

    const unsigned char first_src[3] = {1, 2, 3};
    const unsigned char second_src[3] = {4, 5, 6};
    const unsigned char *first = pinned_colony_insert(colony, first_src);
    const unsigned char *second = pinned_colony_insert(colony, second_src);
    REQUIRE(first != NULL);
    REQUIRE(second == first + 4);
    REQUIRE(first[0] == 1 && first[1] == 2 && first[2] == 3);
    REQUIRE(second[0] == 4 && second[1] == 5 && second[2] == 6);

    void *acquired = pinned_colony_acquire(colony);
    REQUIRE(acquired == second + 4);
    memset(acquired, 0x7E, 3);
    REQUIRE(second[0] == 4);

    pinned_colony_destroy(colony);
}
