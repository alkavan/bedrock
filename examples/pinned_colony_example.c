// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  define _POSIX_C_SOURCE 200809L
#  include <time.h>
#endif

#include <bedrock.h>
#include <bedrock/pinned_colony.h>

#include <stdio.h>
#include <stdlib.h>

typedef struct Record {
    unsigned id;
    unsigned salt;
    struct Record *link;
} Record;

typedef struct Saved {
    Record *ptr;
    unsigned id;
    unsigned salt;
    Record *link;
    unsigned link_id;
    int link_live;
} Saved;

static double seconds_now(void) {
#ifdef _WIN32
    static LARGE_INTEGER freq = {0};
    LARGE_INTEGER now;
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
    }
    QueryPerformanceCounter(&now);
    return (double)now.QuadPart / (double)freq.QuadPart;
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec + (double)now.tv_nsec / 1e9;
#endif
}

int main(void) {
    enum { CAPACITY = 8192, COUNT = 4000 };
    const double spawn_start = seconds_now();

    PinnedColony *colony = pinned_colony_create(CAPACITY, sizeof(Record), _Alignof(Record), NULL, NULL);
    Saved *saved = calloc(COUNT, sizeof(*saved));
    if (!colony || !saved) {
        fprintf(stderr, "pinned_colony example: setup failed\n");
        pinned_colony_destroy(colony);
        free(saved);
        return EXIT_FAILURE;
    }

    for (unsigned id = 0; id < COUNT; ++id) {
        Record record = {.id = id, .salt = 0xC0FFEE00u ^ id, .link = NULL};
        Record *slot = pinned_colony_insert(colony, &record);
        if (!slot) {
            fprintf(stderr, "pinned_colony example: insert failed\n");
            pinned_colony_destroy(colony);
            free(saved);
            return EXIT_FAILURE;
        }
        saved[id].ptr = slot;
        saved[id].id = id;
        saved[id].salt = slot->salt;
    }

    for (unsigned id = 0; id < COUNT; ++id) {
        Record *link = saved[(id + 997u) % COUNT].ptr;
        saved[id].ptr->link = link;
        saved[id].link = link;
        saved[id].link_id = link->id;
        saved[id].link_live = 1;
    }

    const double spawn_seconds = seconds_now() - spawn_start;
    const Record *last_hole = NULL;
    size_t erased = 0;
    for (unsigned id = 0; id < COUNT; ++id) {
        if (id % 5u != 0u) {
            continue;
        }
        last_hole = saved[id].ptr;
        if (pinned_colony_erase(colony, saved[id].ptr) != 0) {
            fprintf(stderr, "pinned_colony example: erase failed\n");
            pinned_colony_destroy(colony);
            free(saved);
            return EXIT_FAILURE;
        }
        saved[id].ptr = NULL;
        ++erased;
    }

    for (unsigned id = 0; id < COUNT; ++id) {
        if (!saved[id].link_live) {
            continue;
        }
        const unsigned link_id = saved[id].link_id;
        if (link_id % 5u == 0u) {
            saved[id].link_live = 0;
        }
    }

    const Record fresh = {.id = COUNT, .salt = 0xC0FFEE00u ^ COUNT, .link = NULL};
    const Record *reused = pinned_colony_insert(colony, &fresh);
    if (reused != last_hole) {
        fprintf(stderr, "pinned_colony example: hole was not reused\n");
        pinned_colony_destroy(colony);
        free(saved);
        return EXIT_FAILURE;
    }

    const double scan_start = seconds_now();
    size_t checked = 0;
    for (unsigned id = 0; id < COUNT; ++id) {
        if (!saved[id].ptr) {
            continue;
        }
        const Record *record = saved[id].ptr;
        if (record->id != saved[id].id || record->salt != saved[id].salt || record->link != saved[id].link) {
            fprintf(stderr, "pinned_colony example: record %u moved or changed\n", id);
            pinned_colony_destroy(colony);
            free(saved);
            return EXIT_FAILURE;
        }
        if (saved[id].link_live && record->link->id != saved[id].link_id) {
            fprintf(stderr, "pinned_colony example: link from %u is no longer record %u\n",
                    id, saved[id].link_id);
            pinned_colony_destroy(colony);
            free(saved);
            return EXIT_FAILURE;
        }
        ++checked;
    }
    const double scan_seconds = seconds_now() - scan_start;

    printf("pinned_colony example — Bedrock v%s\n", BEDROCK_VERSION);
    printf("spawned %d records in %.6f seconds\n", COUNT, spawn_seconds);
    printf("erased %zu records, reused the last hole, checked %zu stable pointers in %.6f seconds\n",
           erased, checked, scan_seconds);

    pinned_colony_destroy(colony);
    free(saved);
    return EXIT_SUCCESS;
}
