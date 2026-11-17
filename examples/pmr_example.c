// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#include <bedrock/pmr.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct CountingResource {
    PmrResource base; /* must be first */
    size_t live_blocks;
    size_t live_bytes;
} CountingResource;

static void *counting_allocate(const PmrResource *self,
                               const size_t bytes, const size_t alignment) {
    CountingResource *counting = (CountingResource *)self;
    void *block = pmr_allocate(pmr_new_delete_resource(), bytes, alignment);
    if (block == NULL) {
        return NULL;
    }

    counting->live_blocks += 1;
    counting->live_bytes += bytes;

    return block;
}

static void counting_deallocate(const PmrResource *self, void *p,
                                const size_t bytes, const size_t alignment) {
    CountingResource *counting = (CountingResource *)self;
    pmr_deallocate(pmr_new_delete_resource(), p, bytes, alignment);
    counting->live_blocks -= 1;
    counting->live_bytes -= bytes;
}

static const PmrResourceVtable counting_vtable = {
    .allocate = counting_allocate,
    .deallocate = counting_deallocate,
    .is_equal = pmr_is_same_object
};

int main(void) {
    CountingResource counting = {
        .base.vtable = &counting_vtable,
        .live_blocks = 0,
        .live_bytes = 0
    };

    const PmrAllocator allocator = pmr_allocator_from(&counting.base);
    unsigned char *block = pmr_allocator_allocate(allocator, 64, _Alignof(max_align_t));

    if (block == NULL) {
        fprintf(stderr, "pmr example: allocate failed\n");
        return EXIT_FAILURE;
    }

    memset(block, 0x5A, 64);
    printf("pmr example: live blocks %u, live bytes %u\n",
           (unsigned)counting.live_blocks, (unsigned)counting.live_bytes);

    pmr_allocator_deallocate(allocator, block, 64, _Alignof(max_align_t));
    if (counting.live_blocks != 0 || counting.live_bytes != 0) {
        fprintf(stderr, "pmr example: block still live\n");
        return EXIT_FAILURE;
    }

    puts("pmr example: allocated and freed 64 bytes through a counting resource");
    return EXIT_SUCCESS;
}
