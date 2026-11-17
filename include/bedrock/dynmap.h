// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#ifndef BEDROCK_DYNMAP_H
#define BEDROCK_DYNMAP_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifdef DYNMAP_THREAD_SAFE
#include <pthread.h>
#endif

/* Allow overrides for C library allocation functions if needed */
#ifndef dynmap_clib_free
#define dynmap_clib_free free
#endif
#ifndef dynmap_clib_malloc
#define dynmap_clib_malloc malloc
#endif
#ifndef dynmap_clib_calloc
#define dynmap_clib_calloc calloc
#endif
#ifndef dynmap_clib_realloc
#define dynmap_clib_realloc realloc
#endif

#define LOAD_FACTOR_THRESHOLD 0.75

/**
 * @struct DynMap
 * @brief Generic dynamic hash map with separate chaining.
 *
 * Stores key-value pairs in an array of buckets. Collisions are resolved
 * via linked lists. Optional user-supplied hash, compare, copy, and destroy
 * callbacks manage key and value lifetime. An optional mutex is available
 * when built with DYNMAP_THREAD_SAFE.
 */
typedef struct DynMap {
    size_t num_buckets;                         /**< Number of hash buckets currently allocated. */
    size_t num_entries;                         /**< Number of key-value entries currently stored. */
    struct DynMapEntry** buckets;               /**< Array of bucket heads (linked lists of entries). */
    unsigned long (*hash)(const void*);         /**< Hash function for keys. */
    int (*compare)(const void*, const void*);   /**< Key comparison; 0 means equal. */
    void* (*key_copy)(const void*);             /**< Copies a key for ownership by the map. */
    void (*key_destroy)(void*);                 /**< Destroys a key owned by the map. */
    void* (*value_copy)(const void*);           /**< Copies a value for ownership by the map. */
    void (*value_destroy)(void*);               /**< Destroys a value owned by the map. */
#ifdef DYNMAP_THREAD_SAFE
    pthread_mutex_t mutex; /* Optional mutex for thread-safety (requires -lpthread) */
#endif
} DynMap;

/**
 * @struct DynMapEntry
 * @brief Represents an individual entry in a hash map.
 *
 * This structure defines a key-value pair as part of a linked-list bucket in a
 * hash map. Each entry contains a pointer to the key, a pointer to the value,
 * and a pointer to the next entry in the bucket, enabling collision resolution
 * via chaining.
 */
typedef struct DynMapEntry {
    void* key;                  /**< Pointer to the key associated with this entry. */
    void* value;                /**< Pointer to the value associated with this entry. */
    struct DynMapEntry* next;   /**< Pointer to the next entry in the linked list for the same hash bucket. */
} DynMapEntry;

static void dynmap_resize(DynMap* map);

/**
 * @brief Initializes a DynMap instance.
 *
 * Sets the initial bucket count, clears the entry count, allocates the
 * bucket array, and installs the hash/compare/copy/destroy callbacks.
 * When built with DYNMAP_THREAD_SAFE, also initializes the map mutex.
 *
 * @param map            Map instance to initialize (must not be NULL).
 * @param buffer_size    Initial number of hash buckets.
 * @param hash           Hash function for keys.
 * @param compare        Key comparison function; return 0 when keys are equal.
 * @param key_copy       Function that copies a key for map ownership.
 * @param key_destroy    Function that destroys a key owned by the map.
 * @param value_copy     Function that copies a value for map ownership.
 * @param value_destroy  Function that destroys a value owned by the map.
 *
 * @note The caller must eventually call dynmap_destroy() to free resources.
 */
static void dynmap_init(DynMap* map, const size_t buffer_size,
                        unsigned long (*hash)(const void*),
                        int (*compare)(const void*, const void*),
                        void* (*key_copy)(const void*),
                        void (*key_destroy)(void*),
                        void* (*value_copy)(const void*),
                        void (*value_destroy)(void*)) {
    map->num_buckets = buffer_size;
    map->num_entries = 0;
    map->buckets = dynmap_clib_calloc(buffer_size, sizeof(DynMapEntry*));
    map->hash = hash;
    map->compare = compare;
    map->key_copy = key_copy;
    map->key_destroy = key_destroy;
    map->value_copy = value_copy;
    map->value_destroy = value_destroy;
#ifdef DYNMAP_THREAD_SAFE
    pthread_mutex_init(&map->mutex, NULL);
#endif
}

/**
 * @brief Doubles the number of buckets and rehashes all entries.
 *
 * Allocates a new bucket array at twice the current size, reinserts every
 * existing entry into its new bucket, then replaces the old array. On
 * allocation failure the map is left unchanged.
 *
 * @param map  Map to resize (must not be NULL and must be initialized).
 *
 * @note Intended for internal use when the load factor exceeds the threshold.
 */
static void dynmap_resize(DynMap* map) {
    const size_t new_size = map->num_buckets * 2;
    DynMapEntry** new_buckets = dynmap_clib_calloc(new_size, sizeof(DynMapEntry*));
    if (!new_buckets) return; /* Failed to allocate, keep old size */

    /* Rehash all entries */
    for (size_t i = 0; i < map->num_buckets; i++) {
        DynMapEntry* e = map->buckets[i];
        while (e) {
            DynMapEntry* next = e->next;
            const unsigned long h = map->hash(e->key) % new_size;
            e->next = new_buckets[h];
            new_buckets[h] = e;
            e = next;
        }
    }

    dynmap_clib_free(map->buckets);
    map->buckets = new_buckets;
    map->num_buckets = new_size;
}

/**
 * @brief Inserts or updates a key-value pair in the map.
 *
 * If the load factor would exceed the threshold, the map is resized first.
 * When @p key already exists, the old value is destroyed and replaced.
 * Otherwise, a new entry is allocated and linked into the target bucket.
 * Keys and values are stored via the map's copy callbacks.
 *
 * @param map    Map to modify (must not be NULL and must be initialized).
 * @param key    Key to insert or update (passed to hash/compare/key_copy).
 * @param value  Value to associate with @p key (passed to value_copy).
 *
 * @return 1 if a new entry was inserted,
 *         0 if an existing entry's value was updated,
 *        -1 if allocation failed.
 *
 * @note Thread-safe when built with DYNMAP_THREAD_SAFE.
 */
static int dynmap_insert(DynMap* map, const void* key, const void* value) {
#ifdef DYNMAP_THREAD_SAFE
    pthread_mutex_lock(&map->mutex);
#endif
    const float load_factor = (float)(map->num_entries + 1) / (float)map->num_buckets;
    if (load_factor > LOAD_FACTOR_THRESHOLD) {
        dynmap_resize(map);
    }

    const unsigned long h = map->hash(key) % map->num_buckets;
    DynMapEntry* e = map->buckets[h];

    while (e) {
        if (map->compare(e->key, key) == 0) {
            map->value_destroy(e->value);
            e->value = map->value_copy(value);
#ifdef DYNMAP_THREAD_SAFE
            pthread_mutex_unlock(&map->mutex);
#endif
            return 0; /* Key already exists, value updated */
        }
        e = e->next;
    }

    DynMapEntry* new_entry = dynmap_clib_malloc(sizeof(DynMapEntry));
    if (!new_entry) {
#ifdef DYNMAP_THREAD_SAFE
        pthread_mutex_unlock(&map->mutex);
#endif
        return -1;
    }
    new_entry->key = map->key_copy(key);
    new_entry->value = map->value_copy(value);
    new_entry->next = map->buckets[h];
    map->buckets[h] = new_entry;
    map->num_entries++;
#ifdef DYNMAP_THREAD_SAFE
    pthread_mutex_unlock(&map->mutex);
#endif
    return 1; /* New insertion */
}

/**
 * @brief Looks up the value associated with a key.
 *
 * Hashes @p key, walks the corresponding bucket chain, and returns the
 * value of the first entry whose key compares equal.
 *
 * @param map  Map to query (must not be NULL and must be initialized).
 * @param key  Key to search for (passed to hash/compare).
 *
 * @return Pointer to the stored value if found, or NULL if @p key is absent.
 *
 * @note The returned pointer refers to map-owned storage; it becomes invalid
 *       if the entry is updated, deleted, or the map is destroyed.
 * @note Thread-safe when built with DYNMAP_THREAD_SAFE.
 */
static void* dynmap_get(const DynMap* map, const void* key) {
#ifdef DYNMAP_THREAD_SAFE
    pthread_mutex_lock((pthread_mutex_t*)&map->mutex);
#endif
    const unsigned long h = map->hash(key) % map->num_buckets;
    const DynMapEntry* e = map->buckets[h];

    while (e) {
        if (map->compare(e->key, key) == 0) {
#ifdef DYNMAP_THREAD_SAFE
            pthread_mutex_unlock((pthread_mutex_t*)&map->mutex);
#endif
            return e->value;
        }
        e = e->next;
    }
#ifdef DYNMAP_THREAD_SAFE
    pthread_mutex_unlock((pthread_mutex_t*)&map->mutex);
#endif
    return NULL; /* Not found */
}

/**
 * @brief Removes the entry associated with a key.
 *
 * Hashes @p key, walks the corresponding bucket chain, and unlinks the
 * first matching entry. The entry's key and value are destroyed via the
 * map callbacks, then the entry node is freed.
 *
 * @param map  Map to modify (must not be NULL and must be initialized).
 * @param key  Key of the entry to remove (passed to hash/compare).
 *
 * @return 1 if an entry was deleted, 0 if @p key was not found.
 *
 * @note Thread-safe when built with DYNMAP_THREAD_SAFE.
 */
static int dynmap_delete(DynMap* map, const void* key) {
#ifdef DYNMAP_THREAD_SAFE
    pthread_mutex_lock(&map->mutex);
#endif
    const unsigned long h = map->hash(key) % map->num_buckets;
    DynMapEntry* e = map->buckets[h];
    DynMapEntry* prev = NULL;

    while (e) {
        if (map->compare(e->key, key) == 0) {
            if (prev) {
                prev->next = e->next;
            } else {
                map->buckets[h] = e->next;
            }
            map->key_destroy(e->key);
            map->value_destroy(e->value);
            dynmap_clib_free(e);
            map->num_entries--;
#ifdef DYNMAP_THREAD_SAFE
            pthread_mutex_unlock(&map->mutex);
#endif
            return 1; /* Deleted */
        }
        prev = e;
        e = e->next;
    }
#ifdef DYNMAP_THREAD_SAFE
    pthread_mutex_unlock(&map->mutex);
#endif
    return 0; /* Not found */
}

/**
 * @brief Frees all resources associated with the map.
 *
 * Walks every bucket, destroys each entry's key and value via the map
 * callbacks, frees entry nodes and the bucket array, then resets counts.
 * When built with DYNMAP_THREAD_SAFE, also destroys the map mutex.
 * A NULL @p map is ignored.
 *
 * @param map  Map to destroy, or NULL.
 *
 * @note After this call the map must not be used unless re-initialized
 *       with dynmap_init().
 */
static void dynmap_destroy(DynMap* map) {
    if (!map) return;
#ifdef DYNMAP_THREAD_SAFE
    pthread_mutex_lock(&map->mutex);
#endif
    for (size_t i = 0; i < map->num_buckets; i++) {
        DynMapEntry* e = map->buckets[i];
        while (e) {
            DynMapEntry* next = e->next;
            map->key_destroy(e->key);
            map->value_destroy(e->value);
            dynmap_clib_free(e);
            e = next;
        }
    }
    dynmap_clib_free(map->buckets);
    map->buckets = NULL;
    map->num_buckets = 0;
    map->num_entries = 0;
#ifdef DYNMAP_THREAD_SAFE
    pthread_mutex_unlock(&map->mutex);
    pthread_mutex_destroy(&map->mutex);
#endif
}

/* These helpers are optional. A translation unit may use one key type, both,
   or neither. */
#if defined(__GNUC__) || defined(__clang__)
#define BD_DYNMAP_UNUSED __attribute__((unused))
#else
#define BD_DYNMAP_UNUSED
#endif
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4505)
#endif

// ---- Utility functions for string keys --------------------------------------

/**
 * @brief djb2 hash for a NUL-terminated string key.
 */
BD_DYNMAP_UNUSED static unsigned long string_hash(const void* key) {
    const char* str = (const char*)key;
    unsigned long hash = 5381;
    int c;
    while ((c = (unsigned char)*str++)) {
        hash = ((hash << 5) + hash) + (unsigned long)c; /* hash * 33 + c */
    }
    return hash;
}

/**
 * @brief Compares two string keys with strcmp.
 */
BD_DYNMAP_UNUSED static int string_compare(const void* a, const void* b) {
    return strcmp((const char*)a, (const char*)b);
}

/**
 * @brief Allocates and copies a string key.
 */
BD_DYNMAP_UNUSED static void* string_copy(const void* key) {
    if (!key) return NULL;

    const size_t len = strlen((const char*)key) + 1;
    char* key_copy = dynmap_clib_malloc(len);
    if (key_copy) {
        memcpy(key_copy, key, len);
    }
    return key_copy;
}

/**
 * @brief Frees a string key allocated by string_copy.
 */
BD_DYNMAP_UNUSED static void string_destroy(void* key) {
    dynmap_clib_free(key);
}

// ---- Utility functions for int keys -----------------------------------------

/**
 * @brief Hashes an int key by casting its value.
 */
BD_DYNMAP_UNUSED static unsigned long int_hash(const void* key) {
    return (unsigned long)*(const int*)key;
}

/**
 * @brief Compares two int keys; returns <0, 0, or >0.
 */
BD_DYNMAP_UNUSED static int int_compare(const void* a, const void* b) {
    const int ia = *(const int*)a;
    const int ib = *(const int*)b;
    return (ia > ib) - (ia < ib);
}

/**
 * @brief Allocates and copies an int key.
 */
BD_DYNMAP_UNUSED static void* int_copy(const void* key) {
    int* new_key = dynmap_clib_malloc(sizeof(int));
    if (new_key) {
        *new_key = *(const int*)key;
    }
    return new_key;
}

/**
 * @brief Frees an int key allocated by int_copy.
 */
BD_DYNMAP_UNUSED static void int_destroy(void* key) {
    dynmap_clib_free(key);
}

// ---- No-op value functions --------------------------------------------------

/**
 * @brief Returns the value pointer unchanged (no allocation).
 */
BD_DYNMAP_UNUSED static void* no_op_copy(const void* value) {
    return (void*)value;
}

/**
 * @brief No-op destructor for values not owned by the map.
 */
BD_DYNMAP_UNUSED static void no_op_destroy(void* value) { /* Don't make value a const!! */
    (void)value; /* Do nothing */
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#undef BD_DYNMAP_UNUSED

/**
 * @def DEFINE_STRING_KEY_MAP(map_name, value_type)
 * @brief Defines a typed hash map with string keys and pointer values.
 *
 * Values are stored by pointer (no_op copy/destroy); the caller owns them.
 *
 * @param map_name    Name of the generated map type and function prefix.
 * @param value_type  Element type pointed to by stored values.
 */
#define DEFINE_STRING_KEY_MAP(map_name, value_type) \
    typedef struct { \
        DynMap map; \
    } map_name; \
    static inline void map_name##_init(map_name* m, size_t buffer_size) { \
        dynmap_init(&m->map, buffer_size, string_hash, string_compare, \
                    string_copy, string_destroy, no_op_copy, no_op_destroy); \
    } \
    static inline int map_name##_insert(map_name* m, const char* key, value_type* value) { \
        return dynmap_insert(&m->map, key, value); \
    } \
    static inline value_type* map_name##_get(map_name* m, const char* key) { \
        return (value_type*)dynmap_get(&m->map, key); \
    } \
    static inline int map_name##_delete(map_name* m, const char* key) { \
        return dynmap_delete(&m->map, key); \
    } \
    static inline void map_name##_destroy(map_name* m) { \
        dynmap_destroy(&m->map); \
    }

/**
 * @def DEFINE_INT_KEY_MAP(map_name, value_type)
 * @brief Defines a typed hash map with int keys and pointer values.
 *
 * Values are stored by pointer (no_op copy/destroy); the caller owns them.
 *
 * @param map_name    Name of the generated map type and function prefix.
 * @param value_type  Element type pointed to by stored values.
 */
#define DEFINE_INT_KEY_MAP(map_name, value_type) \
    typedef struct { \
        DynMap map; \
    } map_name; \
    static inline void map_name##_init(map_name* m, size_t buffer_size) { \
        dynmap_init(&m->map, buffer_size, int_hash, int_compare, \
                    int_copy, int_destroy, no_op_copy, no_op_destroy); \
    } \
    static inline int map_name##_insert(map_name* m, int key, value_type* value) { \
        int tmp_key = key; \
        return dynmap_insert(&m->map, &tmp_key, value); \
    } \
    static inline value_type* map_name##_get(map_name* m, int key) { \
        int tmp_key = key; \
        return (value_type*)dynmap_get(&m->map, &tmp_key); \
    } \
    static inline int map_name##_delete(map_name* m, int key) { \
        int tmp_key = key; \
        return dynmap_delete(&m->map, &tmp_key); \
    } \
    static inline void map_name##_destroy(map_name* m) { \
        dynmap_destroy(&m->map); \
    }

#endif /* BEDROCK_DYNMAP_H */
