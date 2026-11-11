// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "bedrock/pinned_colony.h"
#include "bedrock/vmem.h"

#include <stdlib.h>
#include <string.h>

/*
 * Jump-counting skipfield.
 *
 * A live slot stores 0. A contiguous erased run [start, start + len) stores
 * skipfield[start + i] = len - i, the distance from that slot to the first
 * slot after the run. Forward iteration jumps a hole with one addition.
 * Holes are also threaded through a LIFO free list so insert reuses them
 * before the high-water mark grows. Element pages stay reserved until a
 * slot in that prefix is used.
 */

struct PinnedColony {
    size_t capacity;
    size_t size;
    size_t high_water;
    size_t free_head;
    size_t elem_size;
    size_t stride;
    PinnedColonyDtor dtor;
    void *dtor_user;
    unsigned char *data;
    size_t *skipfield;
    size_t *free_next;
    VMemoryRegion data_region;
    VMemoryRegion skip_region;
    VMemoryRegion free_region;
};

static int is_alive(const PinnedColony *colony, const size_t index) {
    return index < colony->high_water && colony->skipfield[index] == 0;
}

static void *ptr_at(const PinnedColony *colony, const size_t index) {
    return colony->data + index * colony->stride;
}

static void rewrite_run(const PinnedColony *colony, const size_t start, const size_t len) {
    for (size_t i = 0; i < len; ++i) {
        colony->skipfield[start + i] = len - i;
    }
}

static size_t run_start(const PinnedColony *colony, size_t index) {
    while (index > 0 && colony->skipfield[index - 1] != 0) {
        --index;
    }
    return index;
}

static void mark_alive(const PinnedColony *colony, const size_t index) {
    if (colony->skipfield[index] == 0) {
        return;
    }

    const size_t run_end = index + colony->skipfield[index];
    const size_t start = run_start(colony, index);
    colony->skipfield[index] = 0;

    if (index > start) {
        rewrite_run(colony, start, index - start);
    }
    if (run_end > index + 1) {
        rewrite_run(colony, index + 1, run_end - (index + 1));
    }
}

static void mark_erased(const PinnedColony *colony, const size_t index) {
    size_t start = index;
    size_t end = index + 1;

    if (index > 0 && colony->skipfield[index - 1] != 0) {
        start = run_start(colony, index - 1);
    }
    if (end < colony->high_water && colony->skipfield[end] != 0) {
        end += colony->skipfield[end];
    }
    rewrite_run(colony, start, end - start);
}

static size_t next_alive(const PinnedColony *colony, const size_t start) {
    if (start >= colony->high_water) {
        return SIZE_MAX;
    }
    if (colony->skipfield[start] == 0) {
        return start;
    }

    const size_t jumped = start + colony->skipfield[start];
    if (jumped >= colony->high_water) {
        return SIZE_MAX;
    }
    return jumped;
}

static size_t prev_alive(const PinnedColony *colony, size_t from) {
    if (from == SIZE_MAX) {
        from = colony->high_water;
    }
    if (from == 0 || from > colony->high_water) {
        return SIZE_MAX;
    }

    size_t index = from;
    while (index > 0) {
        --index;
        if (colony->skipfield[index] == 0) {
            return index;
        }
    }
    return SIZE_MAX;
}

static void release_slot(PinnedColony *colony, const size_t index) {
    colony->free_next[index] = colony->free_head;
    colony->free_head = index;
}

static int allocate_slot(PinnedColony *colony, size_t *out) {
    if (colony->free_head != SIZE_MAX) {
        const size_t index = colony->free_head;
        colony->free_head = colony->free_next[index];
        colony->free_next[index] = SIZE_MAX;
        *out = index;
        return 0;
    }
    if (colony->high_water >= colony->capacity) {
        return -1;
    }

    const size_t index = colony->high_water++;
    colony->skipfield[index] = 1;
    *out = index;
    return 0;
}

static int commit_up_to(const PinnedColony *colony, const size_t index) {
    if (!colony->data || index >= colony->capacity) {
        return -1;
    }

    size_t end = index + 64u;
    if (end > colony->capacity) {
        end = colony->capacity;
    }
    return vmem_commit(colony->data, end * colony->stride);
}

static void destroy_at(const PinnedColony *colony, const size_t index) {
    if (colony->dtor) {
        colony->dtor(ptr_at(colony, index), colony->dtor_user);
    }
}

static size_t index_of(const PinnedColony *colony, const void *element) {
    if (!element || !colony->data || colony->stride == 0) {
        return SIZE_MAX;
    }

    const uintptr_t addr = (uintptr_t)element;
    const uintptr_t begin = (uintptr_t)colony->data;
    const size_t span = colony->capacity * colony->stride;
    if (addr < begin || addr - begin >= span) {
        return SIZE_MAX;
    }

    const size_t offset = (size_t)(addr - begin);
    if (offset % colony->stride != 0) {
        return SIZE_MAX;
    }

    const size_t index = offset / colony->stride;
    if (!is_alive(colony, index)) {
        return SIZE_MAX;
    }
    return index;
}

PinnedColony *pinned_colony_create(const size_t capacity, const size_t elem_size,
                                    size_t elem_align, const PinnedColonyDtor dtor,
                                    void *user) {
    if (elem_size == 0) {
        return NULL;
    }
    if (elem_align == 0) {
        elem_align = 1;
    }
    if ((elem_align & (elem_align - 1u)) != 0u) {
        return NULL;
    }
    if (elem_align > vmem_page_size()) {
        return NULL;
    }
    if (elem_size > SIZE_MAX - (elem_align - 1u)) {
        return NULL;
    }

    const size_t stride = (elem_size + elem_align - 1u) & ~(elem_align - 1u);
    if (capacity > 0) {
        if (stride > SIZE_MAX / capacity || sizeof(size_t) > SIZE_MAX / capacity) {
            return NULL;
        }
    }

    PinnedColony *colony = calloc(1, sizeof(*colony));
    if (!colony) {
        return NULL;
    }

    colony->capacity = capacity;
    colony->free_head = SIZE_MAX;
    colony->elem_size = elem_size;
    colony->stride = stride;
    colony->dtor = dtor;
    colony->dtor_user = user;

    if (capacity == 0) {
        return colony;
    }

    const size_t elem_bytes = capacity * stride;
    const size_t index_bytes = capacity * sizeof(size_t);
    if (vmem_reserve(&colony->data_region, elem_bytes) != 0 ||
        vmem_reserve(&colony->skip_region, index_bytes) != 0 ||
        vmem_reserve(&colony->free_region, index_bytes) != 0 ||
        vmem_commit(colony->skip_region.base, index_bytes) != 0 ||
        vmem_commit(colony->free_region.base, index_bytes) != 0) {
        vmem_release(&colony->data_region);
        vmem_release(&colony->skip_region);
        vmem_release(&colony->free_region);
        free(colony);
        return NULL;
    }

    colony->data = colony->data_region.base;
    colony->skipfield = colony->skip_region.base;
    colony->free_next = colony->free_region.base;
    for (size_t i = 0; i < capacity; ++i) {
        colony->skipfield[i] = 0;
        colony->free_next[i] = SIZE_MAX;
    }
    return colony;
}

void pinned_colony_destroy(PinnedColony *colony) {
    if (!colony) {
        return;
    }

    for (size_t i = 0; i < colony->high_water; ++i) {
        if (is_alive(colony, i)) {
            destroy_at(colony, i);
        }
    }

    vmem_release(&colony->data_region);
    vmem_release(&colony->skip_region);
    vmem_release(&colony->free_region);
    free(colony);
}

void *pinned_colony_acquire(PinnedColony *colony) {
    if (!colony) {
        return NULL;
    }

    size_t index = 0;
    if (allocate_slot(colony, &index) != 0) {
        return NULL;
    }

    if (commit_up_to(colony, index) != 0) {
        release_slot(colony, index);
        return NULL;
    }

    mark_alive(colony, index);
    ++colony->size;
    return ptr_at(colony, index);
}

void *pinned_colony_insert(PinnedColony *colony, const void *src) {
    if (!colony || !src) {
        return NULL;
    }

    void *slot = pinned_colony_acquire(colony);
    if (!slot) {
        return NULL;
    }
    memcpy(slot, src, colony->elem_size);
    return slot;
}

int pinned_colony_erase(PinnedColony *colony, const void *element) {
    const size_t index = colony ? index_of(colony, element) : SIZE_MAX;
    if (index == SIZE_MAX) {
        return -1;
    }

    destroy_at(colony, index);
    mark_erased(colony, index);
    release_slot(colony, index);
    --colony->size;
    return 0;
}

void pinned_colony_clear(PinnedColony *colony) {
    if (!colony) {
        return;
    }

    for (size_t i = 0; i < colony->high_water; ++i) {
        if (is_alive(colony, i)) {
            destroy_at(colony, i);
        }
        colony->free_next[i] = SIZE_MAX;
    }

    colony->size = 0;
    colony->free_head = SIZE_MAX;
    for (size_t i = 0; i < colony->high_water; ++i) {
        release_slot(colony, i);
    }
    if (colony->high_water > 0) {
        rewrite_run(colony, 0, colony->high_water);
    }
}

size_t pinned_colony_size(const PinnedColony *colony) {
    return colony ? colony->size : 0;
}

size_t pinned_colony_capacity(const PinnedColony *colony) {
    return colony ? colony->capacity : 0;
}

int pinned_colony_empty(const PinnedColony *colony) {
    return !colony || colony->size == 0;
}

void *pinned_colony_at(PinnedColony *colony, const size_t index) {
    if (!colony || !is_alive(colony, index)) {
        return NULL;
    }
    return ptr_at(colony, index);
}

size_t pinned_colony_next_index(const PinnedColony *colony, const size_t index) {
    if (!colony) {
        return SIZE_MAX;
    }
    if (index == SIZE_MAX) {
        return next_alive(colony, 0);
    }
    if (index >= colony->high_water) {
        return SIZE_MAX;
    }
    return next_alive(colony, index + 1);
}

size_t pinned_colony_prev_index(const PinnedColony *colony, const size_t index) {
    if (!colony) {
        return SIZE_MAX;
    }
    if (index == SIZE_MAX) {
        return prev_alive(colony, SIZE_MAX);
    }
    return prev_alive(colony, index);
}

size_t pinned_colony_index(const PinnedColony *colony, const void *element) {
    if (!colony) {
        return SIZE_MAX;
    }
    return index_of(colony, element);
}
