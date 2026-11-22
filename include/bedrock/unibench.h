// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file
 * @brief Register and time a list of benchmarks.
 *
 * `BENCHMARK` adds a display name, tags, and a function to `benchmark_head`
 * before `main`. `run_benchmarks` runs that list in registration order, prints
 * each elapsed time in seconds, and returns the sum.
 */

#ifndef BEDROCK_UNIBENCH_H
#define BEDROCK_UNIBENCH_H

#include "unitest_common.h"

#include <time.h>

#ifdef _WIN32
#include <windows.h>
#undef near
#undef far
#endif

#ifdef __APPLE__
#include <mach/mach_time.h>
#endif

/**
 * @brief One registered benchmark.
 *
 * `BENCHMARK` links these before `main`. `run_benchmarks` walks the list.
 */
struct benchmark_node {
    const char* name;            /**< Display name. */
    const char* tags;            /**< Filterable tags. */
    void (*func)(void);          /**< Function to time. */
    struct benchmark_node* next; /**< Next registered benchmark. */
};

// Global head of the benchmark list
extern struct benchmark_node* benchmark_head;

/**
 * @brief Register a benchmark and open its function body.
 *
 * The next brace block is the benchmark. `name` is pasted into the function,
 * so it must be an identifier.
 *
 * @param name Identifier used as the display name.
 * @param tags String literal of filter tags.
 */
#define BENCHMARK(name, tags)                                                                  \
    static void bench_func_##name(void);                                                       \
    static struct benchmark_node bench_node_##name = { #name, tags, bench_func_##name, NULL }; \
    INITIALIZER(register_bench_##name) {                                                       \
        bench_node_##name.next = benchmark_head;                                               \
        benchmark_head = &bench_node_##name;                                                   \
    }                                                                                          \
    static void bench_func_##name(void)

/**
 * @brief Function to run all registered benchmarks
 * @return Returns the total seconds of all benchmarks summed up
 */
static inline double run_benchmarks(void) {
    double total_time = 0.0;

    // Reverse the benchmark list to run in registration order (first registered)
    struct benchmark_node* prev = NULL;
    struct benchmark_node* current = benchmark_head;
    while (current != NULL) {
        struct benchmark_node* next = current->next;
        current->next = prev;
        prev = current;
        current = next;
    }
    benchmark_head = prev;

    const struct benchmark_node* current_run = benchmark_head;

    // Traverse the list in corrected order
    while (current_run != NULL) {
#ifdef _WIN32
        LARGE_INTEGER freq, start, end;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&start);
        current_run->func();
        QueryPerformanceCounter(&end);
        const double time = (double)(end.QuadPart - start.QuadPart) / freq.QuadPart;
#elif defined(__APPLE__)
        static mach_timebase_info_data_t timebase_info = {0, 0};
        if (timebase_info.denom == 0) {
            mach_timebase_info(&timebase_info);
        }
        uint64_t start = mach_absolute_time();
        current_run->func();
        uint64_t end = mach_absolute_time();
        uint64_t elapsed = end - start;
        uint64_t nano = elapsed * timebase_info.numer / timebase_info.denom;
        double time = (double)nano / 1e9;
#else
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);
        current_run->func();
        clock_gettime(CLOCK_MONOTONIC, &end);
        const double time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
#endif
        printf("BENCHMARK %s: %.9lf seconds\n", current_run->name, time);
        total_time += time;
        current_run = current_run->next;
    }

    return total_time;
}

#endif /* BEDROCK_UNIBENCH_H */
