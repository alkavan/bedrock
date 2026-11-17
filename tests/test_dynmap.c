// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#include <bedrock/dynmap.h>
#include <bedrock/unitest.h>

#ifdef DYNMAP_THREAD_SAFE
#include <pthread.h>
#endif

/* Typed maps used by several tests */
DEFINE_STRING_KEY_MAP(StrIntMap, int)
DEFINE_INT_KEY_MAP(IntStrMap, char)

/* ---- Core DynMap API ---- */

TEST_CASE(test_dynmap_init_and_destroy, "[dynmap]") {
    DynMap map;
    dynmap_init(&map, 8, string_hash, string_compare,
                string_copy, string_destroy,
                no_op_copy, no_op_destroy);

    REQUIRE(map.num_buckets == 8);
    REQUIRE(map.num_entries == 0);
    REQUIRE(map.buckets != NULL);

    dynmap_destroy(&map);
    REQUIRE(map.num_buckets == 0);
    REQUIRE(map.num_entries == 0);
    REQUIRE(map.buckets == NULL);
}

TEST_CASE(test_dynmap_insert_and_get, "[dynmap]") {
    DynMap map;
    dynmap_init(&map, 8, string_hash, string_compare,
                string_copy, string_destroy,
                no_op_copy, no_op_destroy);

    const int value_a = 42;
    const int value_b = 24;

    REQUIRE(dynmap_insert(&map, "alpha", &value_a) == 1);
    REQUIRE(map.num_entries == 1);

    const int *got = dynmap_get(&map, "alpha");
    REQUIRE(got != NULL);
    REQUIRE(*got == 42);

    REQUIRE(dynmap_insert(&map, "beta", &value_b) == 1);
    REQUIRE(map.num_entries == 2);

    got = (int *)dynmap_get(&map, "beta");
    REQUIRE(got != NULL);
    REQUIRE(*got == 24);

    REQUIRE(dynmap_get(&map, "missing") == NULL);

    dynmap_destroy(&map);
}

TEST_CASE(test_dynmap_update_existing_key, "[dynmap]") {
    DynMap map;
    dynmap_init(&map, 8, string_hash, string_compare, string_copy,
        string_destroy, no_op_copy, no_op_destroy);

    const int first = 1;
    const int second = 2;

    REQUIRE(dynmap_insert(&map, "key", &first) == 1);
    REQUIRE(map.num_entries == 1);

    /* Update should return 0 and keep entry count */
    REQUIRE(dynmap_insert(&map, "key", &second) == 0);
    REQUIRE(map.num_entries == 1);

    const int *got = (int *)dynmap_get(&map, "key");
    REQUIRE(got != NULL);
    REQUIRE(*got == 2);

    dynmap_destroy(&map);
}

TEST_CASE(test_dynmap_delete, "[dynmap]") {
    DynMap map;
    dynmap_init(&map, 8, string_hash, string_compare,
                string_copy, string_destroy,
                no_op_copy, no_op_destroy);

    const int value = 99;
    REQUIRE(dynmap_insert(&map, "to-delete", &value) == 1);
    REQUIRE(dynmap_get(&map, "to-delete") != NULL);

    REQUIRE(dynmap_delete(&map, "to-delete") == 1);
    REQUIRE(map.num_entries == 0);
    REQUIRE(dynmap_get(&map, "to-delete") == NULL);

    /* Deleting missing key returns 0 */
    REQUIRE(dynmap_delete(&map, "to-delete") == 0);

    dynmap_destroy(&map);
}

TEST_CASE(test_dynmap_int_keys, "[dynmap]") {
    DynMap map;
    dynmap_init(&map, 8, int_hash, int_compare,int_copy,
        int_destroy, no_op_copy, no_op_destroy);

    const int key1 = 10;
    const int key2 = 20;
    const char *val1 = "ten";
    const char *val2 = "twenty";

    REQUIRE(dynmap_insert(&map, &key1, val1) == 1);
    REQUIRE(dynmap_insert(&map, &key2, val2) == 1);

    const char *got = (char *)dynmap_get(&map, &key1);
    REQUIRE(got != NULL);
    REQUIRE(strcmp(got, "ten") == 0);

    got = (char *)dynmap_get(&map, &key2);
    REQUIRE(got != NULL);
    REQUIRE(strcmp(got, "twenty") == 0);

    const int missing = 999;
    REQUIRE(dynmap_get(&map, &missing) == NULL);

    REQUIRE(dynmap_delete(&map, &key1) == 1);
    REQUIRE(dynmap_get(&map, &key1) == NULL);
    REQUIRE(map.num_entries == 1);

    dynmap_destroy(&map);
}

TEST_CASE(test_dynmap_resize, "[dynmap]") {
    DynMap map;
    /* Small bucket count so inserts cross LOAD_FACTOR_THRESHOLD */
    dynmap_init(&map, 4, string_hash, string_compare, string_copy,
        string_destroy, no_op_copy, no_op_destroy);

    REQUIRE(map.num_buckets == 4);

    int values[8];
    char keys[8][16];
    for (int i = 0; i < 8; ++i) {
        values[i] = i * 10;
        snprintf(keys[i], sizeof(keys[i]), "k%d", i);
        REQUIRE(dynmap_insert(&map, keys[i], &values[i]) == 1);
    }

    /* Load factor exceeded; map should have grown */
    REQUIRE(map.num_buckets > 4);
    REQUIRE(map.num_entries == 8);

    for (int i = 0; i < 8; ++i) {
        const int *got = dynmap_get(&map, keys[i]);
        REQUIRE(got != NULL);
        REQUIRE(*got == values[i]);
    }

    dynmap_destroy(&map);
}

/* ---- DEFINE_*_KEY_MAP macros ---- */

TEST_CASE(test_dynmap_string_key_map_macro, "[dynmap]") {
    StrIntMap m;
    StrIntMap_init(&m, 8);

    int a = 11;
    int b = 22;

    REQUIRE(StrIntMap_insert(&m, "one", &a) == 1);
    REQUIRE(StrIntMap_insert(&m, "two", &b) == 1);

    const int *got = StrIntMap_get(&m, "one");
    REQUIRE(got != NULL);
    REQUIRE(*got == 11);

    got = StrIntMap_get(&m, "two");
    REQUIRE(got != NULL);
    REQUIRE(*got == 22);

    REQUIRE(StrIntMap_get(&m, "nope") == NULL);

    REQUIRE(StrIntMap_delete(&m, "one") == 1);
    REQUIRE(StrIntMap_get(&m, "one") == NULL);

    /* update via re-insert */
    int c = 33;
    REQUIRE(StrIntMap_insert(&m, "two", &c) == 0);
    got = StrIntMap_get(&m, "two");
    REQUIRE(got != NULL);
    REQUIRE(*got == 33);

    StrIntMap_destroy(&m);
}

TEST_CASE(test_dynmap_int_key_map_macro, "[dynmap]") {
    IntStrMap m;
    IntStrMap_init(&m, 8);

    char *hello = "hello";
    char *world = "world";

    REQUIRE(IntStrMap_insert(&m, 1, hello) == 1);
    REQUIRE(IntStrMap_insert(&m, 2, world) == 1);

    const char *got = IntStrMap_get(&m, 1);
    REQUIRE(got != NULL);
    REQUIRE(strcmp(got, "hello") == 0);

    got = IntStrMap_get(&m, 2);
    REQUIRE(got != NULL);
    REQUIRE(strcmp(got, "world") == 0);

    REQUIRE(IntStrMap_get(&m, 99) == NULL);

    REQUIRE(IntStrMap_delete(&m, 1) == 1);
    REQUIRE(IntStrMap_get(&m, 1) == NULL);

    IntStrMap_destroy(&m);
}

TEST_CASE(test_dynmap_multiple_collisions, "[dynmap]") {
    /* Force many entries into few buckets to exercise chaining */
    DynMap map;
    dynmap_init(&map, 2, string_hash, string_compare,
                string_copy, string_destroy, no_op_copy, no_op_destroy);

    int values[16];
    char keys[16][16];
    for (int i = 0; i < 16; ++i) {
        values[i] = i;
        snprintf(keys[i], sizeof(keys[i]), "item-%d", i);
        REQUIRE(dynmap_insert(&map, keys[i], &values[i]) == 1);
    }

    REQUIRE(map.num_entries == 16);

    for (int i = 0; i < 16; ++i) {
        const int *got = (int *)dynmap_get(&map, keys[i]);
        REQUIRE(got != NULL);
        REQUIRE(*got == i);
    }

    /* Delete every other entry */
    for (int i = 0; i < 16; i += 2) {
        REQUIRE(dynmap_delete(&map, keys[i]) == 1);
    }
    REQUIRE(map.num_entries == 8);

    for (int i = 0; i < 16; ++i) {
        const int *got = (int *)dynmap_get(&map, keys[i]);
        if (i % 2 == 0) {
            REQUIRE(got == NULL);
        } else {
            REQUIRE(got != NULL);
            REQUIRE(*got == i);
        }
    }

    dynmap_destroy(&map);
}

#ifdef DYNMAP_THREAD_SAFE
static DynMap *thread_map;

void *thread_insert_func(void *arg) {
    const int thread_id = *(int *)arg;
    for (int i = 0; i < 100; ++i) {
        char key[32];
        snprintf(key, sizeof(key), "t%d-%d", thread_id, i);
        /* value is heap-allocated so each entry owns distinct storage if needed;
           with no_op_copy the pointer is stored as-is — use stack-stable heap ints */
        int *val = malloc(sizeof(int));
        REQUIRE(val != NULL);
        *val = thread_id * 1000 + i;
        const int rc = dynmap_insert(thread_map, key, val);
        REQUIRE(rc == 1);
    }
    return NULL;
}

TEST_CASE(test_dynmap_thread_safety, "[dynmap]") {
    thread_map = malloc(sizeof(DynMap));
    REQUIRE(thread_map != NULL);
    dynmap_init(thread_map, 16, string_hash, string_compare,
                string_copy, string_destroy, no_op_copy, no_op_destroy);

    pthread_t threads[4];
    int thread_ids[4] = {0, 1, 2, 3};

    for (int i = 0; i < 4; ++i) {
        pthread_create(&threads[i], NULL, thread_insert_func, &thread_ids[i]);
    }
    for (int i = 0; i < 4; ++i) {
        pthread_join(threads[i], NULL);
    }

    REQUIRE(thread_map->num_entries == 400);

    /* Spot-check a few keys from each thread */
    for (int t = 0; t < 4; ++t) {
        for (int i = 0; i < 100; i += 25) {
            char key[32];
            snprintf(key, sizeof(key), "t%d-%d", t, i);
            int *got = (int *)dynmap_get(thread_map, key);
            REQUIRE(got != NULL);
            REQUIRE(*got == t * 1000 + i);
        }
    }

    /* Free heap values before destroy (no_op_destroy does not free them) */
    for (int t = 0; t < 4; ++t) {
        for (int i = 0; i < 100; ++i) {
            char key[32];
            snprintf(key, sizeof(key), "t%d-%d", t, i);
            int *got = (int *)dynmap_get(thread_map, key);
            free(got);
        }
    }

    dynmap_destroy(thread_map);
    free(thread_map);
    thread_map = NULL;
}

void *thread_mixed_func(void *arg) {
    const int thread_id = *(int *)arg;
    for (int i = 0; i < 50; ++i) {
        char key[32];
        snprintf(key, sizeof(key), "mix-%d-%d", thread_id, i);

        int *val = malloc(sizeof(int));
        REQUIRE(val != NULL);
        *val = i;
        dynmap_insert(thread_map, key, val);

        int *got = (int *)dynmap_get(thread_map, key);
        REQUIRE(got != NULL);
        REQUIRE(*got == i);

        if (i % 2 == 0) {
            free(got);
            REQUIRE(dynmap_delete(thread_map, key) == 1);
        }
    }
    return NULL;
}

TEST_CASE(test_dynmap_thread_safety_mixed, "[dynmap]") {
    thread_map = malloc(sizeof(DynMap));
    REQUIRE(thread_map != NULL);
    dynmap_init(thread_map, 16, string_hash, string_compare,
                string_copy, string_destroy, no_op_copy, no_op_destroy);

    pthread_t threads[4];
    int thread_ids[4] = {0, 1, 2, 3};

    for (int i = 0; i < 4; ++i) {
        pthread_create(&threads[i], NULL, thread_mixed_func, &thread_ids[i]);
    }
    for (int i = 0; i < 4; ++i) {
        pthread_join(threads[i], NULL);
    }

    /* Odd inserts remain: 25 per thread * 4 = 100 */
    REQUIRE(thread_map->num_entries == 100);

    for (int t = 0; t < 4; ++t) {
        for (int i = 0; i < 50; ++i) {
            char key[32];
            snprintf(key, sizeof(key), "mix-%d-%d", t, i);
            int *got = (int *)dynmap_get(thread_map, key);
            if (i % 2 == 0) {
                REQUIRE(got == NULL);
            } else {
                REQUIRE(got != NULL);
                REQUIRE(*got == i);
                free(got);
            }
        }
    }

    dynmap_destroy(thread_map);
    free(thread_map);
    thread_map = NULL;
}
#endif