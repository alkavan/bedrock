// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#include <stdlib.h>

static int fail_realloc = 0;

static void *test_realloc(void *ptr, size_t size) {
    if (fail_realloc) {
        return NULL;
    }
    return realloc(ptr, size);
}

#define dynvec_clib_realloc test_realloc
#include <bedrock/dynvec.h>
#include <bedrock/unitest.h>

#ifdef DYNVEC_THREAD_SAFE
#include <pthread.h>
#endif

// Helper structure for testing
typedef struct TestElement {
    int value;
} TestElement;

// Destructor for TestElement
static void test_element_destructor(void *elem) {
    if (elem) {
        free(elem);
    }
}

// Copy function for TestElement (deep copy)
static void *test_element_copy(void *elem) {
    if (!elem) return NULL;
    const TestElement *original = elem;
    TestElement *copy = malloc(sizeof(TestElement));
    if (copy) {
        copy->value = original->value;
    }
    return copy;
}

// Test case: Create and free DynVec
TEST_CASE(test_dynvec_new_and_free, "[dynvec]") {
    DynVec *vec = dynvec_new(NULL);
    REQUIRE(vec != NULL);
    REQUIRE(dynvec_length(vec) == 0);
    REQUIRE(dynvec_capacity(vec) == DYNVEC_INITIAL_CAPACITY);
    dynvec_free(vec);
}

// Test case: Push and pop elements
TEST_CASE(test_dynvec_push_pop, "[dynvec]") {
    DynVec *vec = dynvec_new(NULL);
    REQUIRE(vec != NULL);

    TestElement *elem1 = malloc(sizeof(TestElement));
    elem1->value = 42;
    dynvec_push(vec, elem1);
    REQUIRE(dynvec_length(vec) == 1);
    REQUIRE(((TestElement *)dynvec_at(vec, 0))->value == 42);

    TestElement *elem2 = malloc(sizeof(TestElement));
    elem2->value = 24;
    dynvec_push(vec, elem2);
    REQUIRE(dynvec_length(vec) == 2);

    dynvec_pop(vec);
    REQUIRE(dynvec_length(vec) == 1);

    dynvec_free(vec);  // Note: Elements are not freed here since no destructor set
}

// Test case: Insert and erase elements
TEST_CASE(test_dynvec_insert_erase, "[dynvec]") {
    DynVec *vec = dynvec_new(NULL);
    REQUIRE(vec != NULL);

    TestElement *elem1 = malloc(sizeof(TestElement));
    elem1->value = 1;
    dynvec_push(vec, elem1);

    TestElement *elem2 = malloc(sizeof(TestElement));
    elem2->value = 2;
    dynvec_insert(vec, 0, elem2);
    REQUIRE(dynvec_length(vec) == 2);
    REQUIRE(((TestElement *)dynvec_at(vec, 0))->value == 2);
    REQUIRE(((TestElement *)dynvec_at(vec, 1))->value == 1);

    dynvec_erase(vec, 0);
    REQUIRE(dynvec_length(vec) == 1);
    REQUIRE(((TestElement *)dynvec_at(vec, 0))->value == 1);

    dynvec_free(vec);
}

// Test case: Insert and push leave a full vector unchanged when growth fails
TEST_CASE(test_dynvec_insert_skips_when_grow_fails, "[dynvec]") {
    DynVec *vec = dynvec_new(NULL);
    void *elems[DYNVEC_INITIAL_CAPACITY];
    size_t i;
    int extra = 99;

    REQUIRE(vec != NULL);
    for (i = 0; i < DYNVEC_INITIAL_CAPACITY; ++i) {
        elems[i] = malloc(sizeof(int));
        REQUIRE(elems[i] != NULL);
        *(int *)elems[i] = (int)i;
        dynvec_push(vec, elems[i]);
    }
    REQUIRE(dynvec_length(vec) == DYNVEC_INITIAL_CAPACITY);
    REQUIRE(dynvec_capacity(vec) == DYNVEC_INITIAL_CAPACITY);

    fail_realloc = 1;
    dynvec_insert(vec, 0, &extra);
    dynvec_push(vec, &extra);
    fail_realloc = 0;

    REQUIRE(dynvec_length(vec) == DYNVEC_INITIAL_CAPACITY);
    REQUIRE(dynvec_capacity(vec) == DYNVEC_INITIAL_CAPACITY);
    REQUIRE(dynvec_at(vec, 0) == elems[0]);
    REQUIRE(*(int *)dynvec_at(vec, 0) == 0);
    REQUIRE(dynvec_at(vec, DYNVEC_INITIAL_CAPACITY - 1) == elems[DYNVEC_INITIAL_CAPACITY - 1]);

    dynvec_free(vec);
    for (i = 0; i < DYNVEC_INITIAL_CAPACITY; ++i) {
        free(elems[i]);
    }
}

// Test case: Copy DynVec (shallow)
TEST_CASE(test_dynvec_copy_shallow, "[dynvec]") {
    DynVec *vec1 = dynvec_new(NULL);
    TestElement *elem = malloc(sizeof(TestElement));
    elem->value = 100;
    dynvec_push(vec1, elem);

    DynVec *vec2 = dynvec_new(NULL);
    dynvec_copy(vec2, vec1);
    REQUIRE(dynvec_length(vec2) == 1);
    REQUIRE(((TestElement *)dynvec_at(vec2, 0))->value == 100);

    dynvec_free(vec1);
    dynvec_free(vec2);
}

// Test case: Copy DynVec (deep with copy function)
TEST_CASE(test_dynvec_copy_deep, "[dynvec]") {
    DynVec *vec1 = dynvec_new(test_element_destructor);
    dynvec_set_copy_func(vec1, test_element_copy);
    TestElement *elem = malloc(sizeof(TestElement));
    elem->value = 200;
    dynvec_push(vec1, elem);

    DynVec *vec2 = dynvec_new(test_element_destructor);
    dynvec_set_copy_func(vec2, test_element_copy);
    dynvec_copy(vec2, vec1);
    REQUIRE(dynvec_length(vec2) == 1);
    REQUIRE(((TestElement *)dynvec_at(vec2, 0))->value == 200);

    dynvec_free(vec1);  // Should free original elem
    dynvec_free(vec2);  // Should free copied elem
}

// Test case: Reserve capacity
TEST_CASE(test_dynvec_reserve, "[dynvec]") {
    DynVec *vec = dynvec_new(NULL);
    REQUIRE(dynvec_capacity(vec) == DYNVEC_INITIAL_CAPACITY);

    dynvec_reserve(vec, 20);
    REQUIRE(dynvec_capacity(vec) >= 20);

    dynvec_free(vec);
}

// Test case: Empty vector checks
TEST_CASE(test_dynvec_empty, "[dynvec]") {
    DynVec *vec = dynvec_new(NULL);
    REQUIRE(dynvec_empty(vec) != 0);

    TestElement *elem = malloc(sizeof(TestElement));
    elem->value = 0;
    dynvec_push(vec, elem);
    REQUIRE(dynvec_empty(vec) == 0);

    dynvec_free(vec);
}

// Test case: Destructor functionality
TEST_CASE(test_dynvec_destructor, "[dynvec]") {
    DynVec *vec = dynvec_new(test_element_destructor);
    TestElement *elem = malloc(sizeof(TestElement));
    elem->value = 50;
    dynvec_push(vec, elem);

    // Freeing vec should call destructor on elem
    dynvec_free(vec);
}

// Global counter for foreach test
static int foreach_counter = 0;

// Foreach function that increments counter (now const-correct)
static void* test_foreach_func(const void *elem) {
    if (elem) {
        foreach_counter++;
    }
    return NULL;  // Return value is not used in FOR_EACH
}

// Test case: Use foreach function with FOR_EACH macro
TEST_CASE(test_dynvec_foreach, "[dynvec]") {
    DynVec *vec = dynvec_new(NULL);
    REQUIRE(vec != NULL);

    // Add some elements
    TestElement *elem1 = malloc(sizeof(TestElement));
    elem1->value = 10;
    dynvec_push(vec, elem1);

    TestElement *elem2 = malloc(sizeof(TestElement));
    elem2->value = 20;
    dynvec_push(vec, elem2);

    TestElement *elem3 = malloc(sizeof(TestElement));
    elem3->value = 30;
    dynvec_push(vec, elem3);

    REQUIRE(dynvec_length(vec) == 3);

    // Reset counter
    foreach_counter = 0;

    FOR_EACH(vec, test_foreach_func);
    REQUIRE(foreach_counter == 3);

    dynvec_free(vec);
}

// New const foreach test function
static void* test_const_foreach_func(const void *elem) {
    if (elem) {
        const int val = ((TestElement*)elem)->value;
        (void)val;  // Suppress unused warning
    }
    return NULL;
}

// Test case: Const foreach functionality
TEST_CASE(test_dynvec_const_foreach, "[dynvec]") {
    DynVec *vec = dynvec_new(NULL);
    REQUIRE(vec != NULL);

    // Add some elements
    TestElement *elem1 = malloc(sizeof(TestElement));
    elem1->value = 100;
    dynvec_push(vec, elem1);

    TestElement *elem2 = malloc(sizeof(TestElement));
    elem2->value = 200;
    dynvec_push(vec, elem2);

    REQUIRE(dynvec_length(vec) == 2);
    FOR_EACH(vec, test_const_foreach_func);

    dynvec_free(vec);
}

static int clear_destructor_calls = 0;

static void clear_test_destructor(void *elem) {
    clear_destructor_calls++;
    if (elem) {
        free(elem);
    }
}

TEST_CASE(test_dynvec_clear, "[dynvec]") {
    // Test empty clear
    DynVec *vec = dynvec_new(clear_test_destructor);
    REQUIRE(vec != NULL);
    REQUIRE(dynvec_length(vec) == 0);
    REQUIRE(dynvec_empty(vec));

    clear_destructor_calls = 0;
    dynvec_clear(vec);
    REQUIRE(clear_destructor_calls == 0);
    REQUIRE(dynvec_length(vec) == 0);
    REQUIRE(dynvec_capacity(vec) == DYNVEC_INITIAL_CAPACITY);

    // Test clear with elements (verifies destructor calls)
    TestElement *e1 = malloc(sizeof(TestElement));
    e1->value = 1;
    dynvec_push(vec, e1);

    TestElement *e2 = malloc(sizeof(TestElement));
    e2->value = 2;
    dynvec_push(vec, e2);

    REQUIRE(dynvec_length(vec) == 2);

    clear_destructor_calls = 0;
    dynvec_clear(vec);
    REQUIRE(dynvec_length(vec) == 0);
    REQUIRE(clear_destructor_calls == 2);
    REQUIRE(dynvec_capacity(vec) == DYNVEC_INITIAL_CAPACITY);

    // Test reuse after clear
    TestElement *e3 = malloc(sizeof(TestElement));
    e3->value = 3;
    dynvec_push(vec, e3);
    REQUIRE(dynvec_length(vec) == 1);
    REQUIRE(((TestElement *)dynvec_at(vec, 0))->value == 3);

    dynvec_free(vec);
}

#ifdef DYNVEC_THREAD_SAFE
// Thread-safety test data
static DynVec *thread_vec;
static int thread_push_count = 0;

// Thread function for pushing elements
void* thread_push_func(void *arg) {
    for (int i = 0; i < 100; ++i) {
        TestElement *elem = malloc(sizeof(TestElement));
        elem->value = i;
        dynvec_push(thread_vec, elem);
        thread_push_count++;
    }
    return NULL;
}

// Test case: Basic thread-safety check
TEST_CASE(test_dynvec_thread_safety, "[dynvec]") {
    thread_vec = dynvec_new(NULL);
    REQUIRE(thread_vec != NULL);

    pthread_t threads[4];
    thread_push_count = 0;

    // Create 4 threads to push elements concurrently
    for (int i = 0; i < 4; ++i) {
        pthread_create(&threads[i], NULL, thread_push_func, NULL);
    }

    // Wait for threads to finish
    for (int i = 0; i < 4; ++i) {
        pthread_join(threads[i], NULL);
    }

    REQUIRE(dynvec_length(thread_vec) == 400);

    dynvec_free(thread_vec);
}

// Thread function for pushing elements using passed arg (thread ID)
void* thread_push_func_arg(void *arg) {
    const int thread_id = *(int*)arg;
    for (int i = 0; i < 100; ++i) {
        TestElement *elem = malloc(sizeof(TestElement));
        elem->value = i + thread_id * 100;
        dynvec_push(thread_vec, elem);
        thread_push_count++;
    }
    return NULL;
}

// Test case: Thread-safety check with argument passing
TEST_CASE(test_dynvec_thread_safety_arg, "[dynvec]") {
    thread_vec = dynvec_new(NULL);
    REQUIRE(thread_vec != NULL);

    pthread_t threads[4];
    thread_push_count = 0;
    int thread_ids[4] = {0, 1, 2, 3};

    // Create 4 threads to push elements concurrently, passing thread ID as arg
    for (int i = 0; i < 4; ++i) {
        pthread_create(&threads[i], NULL, thread_push_func_arg, &thread_ids[i]);
    }

    // Wait for threads to finish
    for (int i = 0; i < 4; ++i) {
        pthread_join(threads[i], NULL);
    }

    REQUIRE(dynvec_length(thread_vec) == 400);

    // Verify elements have values from correct thread ranges (confirms arg usage)
    int counts[4] = {0};
    for (size_t j = 0; j < dynvec_length(thread_vec); ++j) {
        const TestElement *e = dynvec_at(thread_vec, j);
        REQUIRE(e != NULL);
        const int v = e->value;
        const int id = v / 100;
        if (id >= 0 && id < 4) {
            counts[id]++;
        } else {
            FAIL("Element value outside expected thread range");
        }
    }
    REQUIRE(counts[0] == 100);
    REQUIRE(counts[1] == 100);
    REQUIRE(counts[2] == 100);
    REQUIRE(counts[3] == 100);

    dynvec_free(thread_vec);
}
#endif
