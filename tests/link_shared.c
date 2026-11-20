// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#include <bedrock/arena.h>
#include <bedrock/args.h>
#include <bedrock/parse.h>
#include <bedrock/pinned_colony.h>
#include <bedrock/pmr.h>
#include <bedrock/vmem.h>

int main(void)
{
    MemHeap *heap = arena_create(64);
    ProgramArgs it;
    int64_t value = 0;
    PinnedColony *colony;

    arena_destroy(heap);
    args_init(&it, 0, NULL);

    if (!bd_parse_i64("1", &value) || value != 1) {
        return 1;
    }

    colony = pinned_colony_create(1, sizeof(int), 1, NULL, NULL);
    pinned_colony_destroy(colony);

    if (pmr_new_delete_resource() == NULL || vmem_page_size() == 0) {
        return 1;
    }
    return 0;
}
