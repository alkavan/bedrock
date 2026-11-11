// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#ifndef BEDROCK_DYNVEC_H
#define BEDROCK_DYNVEC_H

#include <stdlib.h> /* for malloc/realloc/free */
#include <string.h> /* for memcpy/memmove */
#include <stddef.h> /* for size_t */

#ifdef DYNVEC_THREAD_SAFE
#include <pthread.h>
#endif

/* Allow overrides for C library allocation functions if needed */
#ifndef dynvec_clib_free
#define dynvec_clib_free free
#endif
#ifndef dynvec_clib_malloc
#define dynvec_clib_malloc malloc
#endif
#ifndef dynvec_clib_realloc
#define dynvec_clib_realloc realloc
#endif

/* Initial capacity for new vectors */
#ifndef DYNVEC_INITIAL_CAPACITY
#define DYNVEC_INITIAL_CAPACITY 8
#endif

/** @brief Callback that destroys a single vector element. */
typedef void (*DynVecDestructor)(void *element);

/** @brief Callback that deep-copies a single vector element. */
typedef void* (*DynVecCopy)(void *element);

/** @brief Callback invoked once per element (e.g. by FOR_EACH). */
typedef void* (*DynVecForEach)(void *element);

/**
 * @brief Dynamic vector of void* element pointers.
 *
 * Stores pointers only; callers own allocation unless a destructor is set.
 * Optional copy_func enables deep copies in dynvec_copy.
 */
typedef struct DynVec {
    size_t capacity;              /**< Allocated slot count. */
    size_t length;                /**< Number of live elements. */
    void **elements;              /**< Contiguous array of element pointers. */
    DynVecDestructor destructor;  /**< Optional per-element destructor. */
    DynVecCopy copy_func;         /**< Optional per-element deep-copy function. */
#ifdef DYNVEC_THREAD_SAFE
    pthread_mutex_t mutex;        /**< Optional mutex (requires -lpthread). */
#endif
} DynVec;

/**
 * @brief Creates a vector with default initial capacity.
 *
 * @param dtor  Optional element destructor, or NULL.
 *
 * @return New vector, or NULL on allocation failure.
 */
static inline DynVec *dynvec_new(const DynVecDestructor dtor) {
    DynVec *self = dynvec_clib_malloc(sizeof(DynVec));
    if (!self) return NULL;
    self->length = 0;
    self->capacity = DYNVEC_INITIAL_CAPACITY;
    self->elements = (void **)dynvec_clib_malloc(sizeof(void *) * self->capacity);
    if (!self->elements) {
        dynvec_clib_free(self);
        return NULL;
    }
    self->destructor = dtor;
    self->copy_func = NULL;
#ifdef DYNVEC_THREAD_SAFE
    pthread_mutex_init(&self->mutex, NULL);
#endif
    return self;
}

/**
 * @brief Creates a vector with a chosen initial capacity.
 *
 * @param dtor      Optional element destructor, or NULL.
 * @param capacity  Initial capacity in elements.
 *
 * @return New vector, or NULL on allocation failure.
 */
static inline DynVec *dynvec_with_capacity(const DynVecDestructor dtor, const size_t capacity) {
    DynVec *self = dynvec_new(dtor);
    if (!self) return NULL;
#ifdef DYNVEC_THREAD_SAFE
    pthread_mutex_destroy(&self->mutex);  // Destroy before realloc/free
#endif
    dynvec_clib_free(self->elements);
    self->elements = (void **)dynvec_clib_malloc(sizeof(void *) * capacity);
    if (!self->elements) {
        dynvec_clib_free(self);
        return NULL;
    }
    self->capacity = capacity;
    self->length = 0;
#ifdef DYNVEC_THREAD_SAFE
    pthread_mutex_init(&self->mutex, NULL);  // Re-init safely
#endif
    return self;
}

/**
 * @brief Sets the element destructor.
 *
 * @param vec   Vector to update.
 * @param dtor  New destructor, or NULL to clear.
 */
static inline void dynvec_set_destructor(DynVec *vec, const DynVecDestructor dtor) {
    if (vec) {
        vec->destructor = dtor;
    }
}

/**
 * @brief Returns the element destructor.
 *
 * @param vec  Vector to query.
 *
 * @return Destructor pointer, or NULL if unset/invalid.
 */
static inline DynVecDestructor dynvec_destructor(const DynVec *vec) {
    return vec ? vec->destructor : NULL;
}

/**
 * @brief Returns the number of elements.
 *
 * @param vec  Vector to query (NULL-safe).
 *
 * @return Length, or 0 if @p vec is NULL.
 */
static inline size_t dynvec_length(const DynVec *vec) {
    return vec ? vec->length : (size_t)0;
}

/**
 * @brief Returns the allocated capacity.
 *
 * @param vec  Vector to query (NULL-safe).
 *
 * @return Capacity, or 0 if @p vec is NULL.
 */
static inline size_t dynvec_capacity(const DynVec *vec) {
    return vec ? vec->capacity : (size_t)0;
}

/**
 * @brief Returns whether the vector has no elements.
 *
 * @param vec  Vector to query.
 *
 * @return Non-zero if empty or @p vec is NULL.
 */
static inline int dynvec_empty(const DynVec *vec) {
    return dynvec_length(vec) == 0;
}

/**
 * @brief Grows capacity to at least `n` if needed.
 *
 * @param vec  Vector to reserve space in.
 * @param n    Minimum required capacity.
 */
static inline void dynvec_reserve(DynVec *vec, const size_t n) {
    if (!vec || n <= vec->capacity) return;
#ifdef DYNVEC_THREAD_SAFE
    pthread_mutex_lock(&vec->mutex);
#endif
    size_t new_cap = vec->capacity;
    while (new_cap < n) {
        new_cap *= 2;
    }
    void **new_elements = dynvec_clib_realloc(vec->elements, sizeof(void *) * new_cap);
    if (new_elements) {
        vec->elements = new_elements;
        vec->capacity = new_cap;
    }
#ifdef DYNVEC_THREAD_SAFE
    pthread_mutex_unlock(&vec->mutex);
#endif
}

/**
 * @brief Doubles capacity when the vector is full.
 *
 * @param vec  Vector to grow.
 */
static void dynvec_grow(DynVec *vec) {
    if (!vec) return;
    size_t new_cap = vec->capacity * 2;
    if (new_cap == 0) new_cap = DYNVEC_INITIAL_CAPACITY;
    void **new_elements = dynvec_clib_realloc(vec->elements, sizeof(void *) * new_cap);
    if (new_elements) {
        vec->elements = new_elements;
        vec->capacity = new_cap;
#ifdef DYNVEC_THREAD_SAFE
        // Zero new padding bytes for safety (thread-safe ops may read beyond length)
        const size_t old_cap = new_cap / 2;  // Previous capacity
        if (vec->length > old_cap) vec->length = old_cap;  // Trim if corrupted
        memset(vec->elements + vec->length, 0, (new_cap - vec->length) * sizeof(void *));
#endif
    }
    // On realloc fail: capacity unchanged, next push will retry grow
}

/**
 * @brief Runs the destructor on elements[i] if one is set.
 *
 * @param vec  Vector owning the element.
 * @param i    Element index.
 */
static void dynvec_maybe_destroy(DynVec *vec, const size_t i) {
    if (!vec || i >= vec->length) return;
    const DynVecDestructor dtor = dynvec_destructor(vec);
    if (dtor) {
        dtor(vec->elements[i]);
    }
}

/**
 * @brief Appends an element pointer, growing if needed.
 *
 * @param vec   Vector to modify.
 * @param elem  Non-NULL pointer to store.
 */
static inline void dynvec_push(DynVec *vec, void *elem) {
    if (!vec || !elem) return;
#ifdef DYNVEC_THREAD_SAFE
    pthread_mutex_lock(&vec->mutex);
#endif
    if (vec->length >= vec->capacity) {
        dynvec_grow(vec);
    }
    if (vec->length < vec->capacity) {
        vec->elements[vec->length++] = elem;
    }
#ifdef DYNVEC_THREAD_SAFE
    pthread_mutex_unlock(&vec->mutex);
#endif
}

/**
 * @brief Inserts an element at position, shifting the tail right.
 *
 * On grow failure the vector is left unchanged.
 *
 * @param vec   Vector to modify.
 * @param pos   Insert index in [0, length].
 * @param elem  Non-NULL pointer to insert.
 */
static inline void dynvec_insert(DynVec *vec, const size_t pos, void *elem) {
    if (!vec || !elem || pos > vec->length) return;
#ifdef DYNVEC_THREAD_SAFE
    pthread_mutex_lock(&vec->mutex);
#endif
    if (vec->length >= vec->capacity) {
        dynvec_grow(vec);
    }
    /* Grow failed: capacity is unchanged, so do not write past the array. */
    if (vec->length < vec->capacity) {
        if (pos < vec->length) {
            memmove(&vec->elements[pos + 1], &vec->elements[pos],
                    (vec->length - pos) * sizeof(void *));
        }
        vec->elements[pos] = elem;
        vec->length++;
    }
#ifdef DYNVEC_THREAD_SAFE
    pthread_mutex_unlock(&vec->mutex);
#endif
}

/**
 * @brief Removes the last element, calling its destructor if set.
 *
 * @param vec  Vector to modify.
 */
static inline void dynvec_pop(DynVec *vec) {
    if (!vec || vec->length == 0) return;
#ifdef DYNVEC_THREAD_SAFE
    pthread_mutex_lock(&vec->mutex);
#endif
    dynvec_maybe_destroy(vec, vec->length - 1);
    vec->length--;
#ifdef DYNVEC_THREAD_SAFE
    pthread_mutex_unlock(&vec->mutex);
#endif
}

/**
 * @brief Erases the element at position, calling its destructor and shifting.
 *
 * @param vec  Vector to modify.
 * @param pos  Index in [0, length).
 */
static inline void dynvec_erase(DynVec *vec, const size_t pos) {
    if (!vec || pos >= vec->length) return;
#ifdef DYNVEC_THREAD_SAFE
    pthread_mutex_lock(&vec->mutex);
#endif
    dynvec_maybe_destroy(vec, pos);
    if (pos < vec->length - 1) {
        memmove(&vec->elements[pos], &vec->elements[pos + 1],
                (vec->length - pos - 1) * sizeof(void *));
    }
    vec->length--;
#ifdef DYNVEC_THREAD_SAFE
    pthread_mutex_unlock(&vec->mutex);
#endif
}

/**
 * @brief Copies elements from one vector to another.
 *
 * Uses @p to->copy_func for deep copy when set; otherwise shallow-copies
 * pointers. Reserves capacity in @p to as needed.
 *
 * @param to    Destination vector.
 * @param from  Source vector.
 */
static inline void dynvec_copy(DynVec *to, const DynVec *from) {
    if (!to || !from) return;
    dynvec_reserve(to, from->length);
    if (to->capacity >= from->length) {
        if (to->copy_func) {
            for (size_t i = 0; i < from->length; ++i) {
                to->elements[i] = to->copy_func(from->elements[i]);
            }
        } else {
            memcpy(to->elements, from->elements, from->length * sizeof(void *));
        }
        to->length = from->length;
    }
}

/**
 * @brief Destroys all elements and frees the vector.
 *
 * @param vec  Vector to free (NULL-safe).
 */
static inline void dynvec_free(DynVec *vec) {
    if (!vec) return;
    const DynVecDestructor dtor = dynvec_destructor(vec);
    if (dtor) {
        for (size_t i = 0; i < vec->length; ++i) {
            dtor(vec->elements[i]);
        }
    }
    dynvec_clib_free(vec->elements);
#ifdef DYNVEC_THREAD_SAFE
    pthread_mutex_destroy(&vec->mutex);
#endif
    dynvec_clib_free(vec);
}

/**
 * @brief Destroys all elements and sets length to 0.
 *
 * Capacity and the elements array are kept.
 *
 * @param vec  Vector to clear.
 */
static inline void dynvec_clear(DynVec *vec) {
    if (!vec) return;
#ifdef DYNVEC_THREAD_SAFE
    pthread_mutex_lock(&vec->mutex);
#endif
    const DynVecDestructor dtor = dynvec_destructor(vec);
    if (dtor) {
        for (size_t i = 0; i < vec->length; ++i) {
            dtor(vec->elements[i]);
        }
    }
    vec->length = 0;
#ifdef DYNVEC_THREAD_SAFE
    pthread_mutex_unlock(&vec->mutex);
#endif
}

/**
 * @brief Returns elements[i] without bounds checking.
 *
 * @param vec  Vector to index.
 * @param i    Element index (must be in range).
 *
 * @return Element pointer, or NULL if @p vec is NULL.
 */
static inline void *dynvec_at_unsafe(const DynVec *vec, const size_t i) {
    return vec ? vec->elements[i] : NULL;
}

/**
 * @brief Returns elements[i], or NULL if out of range.
 *
 * @param vec  Vector to index.
 * @param i    Element index.
 *
 * @return Element pointer, or NULL if invalid.
 */
static inline void *dynvec_at(const DynVec *vec, const size_t i) {
    return (vec && i < vec->length) ? dynvec_at_unsafe(vec, i) : NULL;
}

/**
 * @brief Finds the first index of element by pointer identity (unchecked).
 *
 * @param vec   Non-NULL vector.
 * @param elem  Non-NULL pointer to search for.
 *
 * @return Index if found, or (size_t)-1 if not found.
 */
static inline size_t dynvec_index_of_unsafe(const DynVec *vec, const void *elem) {
    for (size_t i = 0; i < vec->length; ++i) {
        if (vec->elements[i] == elem) {
            return i;
        }
    }
    return (size_t)-1;
}


/**
 * @brief Finds the first index of element by pointer identity.
 *
 * @param vec   Vector to search.
 * @param elem  Pointer to search for.
 *
 * @return Index if found, or (size_t)-1 if missing/invalid.
 */
static inline size_t dynvec_index_of(const DynVec *vec, const void *elem) {
    if (!vec || !elem) return (size_t)-1;
    return dynvec_index_of_unsafe(vec, elem);
}

/**
 * @brief Returns a pointer to the first element slot.
 *
 * @param vec  Vector to query.
 *
 * @return &elements[0], or NULL if empty/invalid.
 */
static inline void **dynvec_begin(const DynVec *vec) {
    return vec && vec->length > 0 ? vec->elements : NULL;
}

/**
 * @brief Returns a pointer one past the last element.
 *
 * @param vec  Vector to query.
 *
 * @return &elements[length], or NULL if @p vec is NULL.
 */
static inline void **dynvec_end(const DynVec *vec) {
    return vec ? vec->elements + vec->length : NULL;
}

/**
 * @brief Sets the element deep-copy function.
 *
 * @param vec        Vector to update.
 * @param copy_func  New copy function, or NULL to clear.
 */
static inline void dynvec_set_copy_func(DynVec *vec, const DynVecCopy copy_func) {
    if (vec) {
        vec->copy_func = copy_func;
    }
}

/**
 * @brief Returns the element deep-copy function.
 *
 * @param vec  Vector to query.
 *
 * @return Copy function, or NULL if unset/invalid.
 */
static inline DynVecCopy dynvec_copy_func(const DynVec *vec) {
    return vec ? vec->copy_func : NULL;
}

/**
 * @def FOR_EACH(vec, func, ...)
 * @brief Calls function for each element in vector.
 *
 * @param vec   Vector to iterate.
 * @param func  Callback taking the element pointer, then optional args.
 * @param ...   Extra arguments forwarded to @p func.
 */
#ifdef DYNVEC_THREAD_SAFE
#define FOR_EACH(vec, func, ...) do { \
    if ((vec) && (func)) { \
        pthread_mutex_lock(&(vec)->mutex); \
        for (size_t i = 0; i < (vec)->length; ++i) { \
            (func)((void *)(vec)->elements[i], ##__VA_ARGS__); \
        } \
        pthread_mutex_unlock(&(vec)->mutex); \
    } \
} while(0)
#else
#define FOR_EACH(vec, func, ...) do { \
    if ((vec) && (func)) { \
        for (size_t i = 0; i < (vec)->length; ++i) { \
            (func)((void *)(vec)->elements[i], ##__VA_ARGS__); \
        } \
    } \
} while(0)
#endif

#endif /* BEDROCK_DYNVEC_H */
