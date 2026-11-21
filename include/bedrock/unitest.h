// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file
 * @brief Register and run a list of tests.
 *
 * `TEST_CASE` adds a display name, tags, and a function to `test_head`
 * before `main`. `run_tests` runs that list, prints each failure and the
 * pass count, and returns the number of failures.
 */

#ifndef BEDROCK_UNITEST_H
#define BEDROCK_UNITEST_H

#include "unitest_common.h"

// Printing
#ifdef UNITEST_PRINT_SUCCESS
#define PRINT_TEST_PASS(name, tags) printf("PASS: %s %s\n", tags, name)
#else
#define PRINT_TEST_PASS(name, tags) /* do nothing */
#endif
#define PRINT_TEST_FAIL(name, tags) printf("FAIL: %s %s\n", tags, name)

/**
 * @brief One registered test.
 *
 * `TEST_CASE` links these before `main`. `run_tests` walks the list.
 */
struct test_node {
    const char* name;       /**< Display name. */
    const char* tags;       /**< Filterable tags. */
    void (*func)(void);     /**< Function to run. */
    struct test_node* next; /**< Next registered test. */
};

// Global head of the test list (declaration)
extern struct test_node* test_head;

// Test Case
#define TEST_CASE(name, tags) \
    static void test_func_##name(void); \
    static struct test_node test_node_##name = { #name, tags, test_func_##name, NULL }; \
    INITIALIZER(register_##name) { \
        test_node_##name.next = test_head; \
        test_head = &test_node_##name; \
    } \
    static void test_func_##name(void)

/**
 * @brief Function to run all registered tests
 * @return Returns the number of failures (0 for success)
 */
static int run_tests(void) {
    int passed = 0;
    int total = 0;
    const struct test_node* current = test_head;

    // Traverse the list (order may be reversed due to registration, but often doesn't matter)
    while (current != NULL) {
        total++;

        // Set up a jump point for this test
        TRY {
            // Run the test function
            current->func();
            // If we get here, the test passed (no THROW)
            PRINT_TEST_PASS(current->name, current->tags);
            passed++;
        } CATCH {
            // THROW occurred, test failed
            PRINT_TEST_FAIL(current->name, current->tags);
        } END_TRY;

        current = current->next;
    }

    printf("All tests completed: %d/%d passed\n", passed, total);
    return total - passed;
}

#endif /* BEDROCK_UNITEST_H */
