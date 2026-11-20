// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "bedrock/arena.h"

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  define _POSIX_C_SOURCE 200809L
#  define _DEFAULT_SOURCE
#  include <sys/mman.h>
#  include <unistd.h>
#  ifndef MAP_ANONYMOUS
#    define MAP_ANONYMOUS MAP_ANON
#  endif
#endif

#include <stdlib.h>
#include <stdint.h>

#define ALIGN 16u
#define HDR_SIZE ALIGN

typedef struct FreeNode {
    size_t offset;
    size_t size;
    struct FreeNode *next;
} FreeNode;

struct MemHeap {
    uint8_t  *base;
    size_t mapped;
    size_t capacity;
    size_t used;
    FreeNode *free_list;
    FreeNode *spare;     /**< reused so a free can be recorded if malloc fails */
};

typedef struct {
    size_t size;
} Header;

static size_t page_size(void)
{
#ifdef _WIN32
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return (info.dwPageSize == 0) ? 4096u : (size_t)info.dwPageSize;
#else
    const long p = sysconf(_SC_PAGESIZE);
    return (p > 0) ? (size_t)p : 4096u;
#endif
}

static size_t align_up(const size_t n, const size_t a)
{
    return (n + a - 1u) & ~(a - 1u);
}

MemHeap *arena_create(const size_t capacity)
{
    if (capacity == 0) {
        return NULL;
    }

    const size_t ps = page_size();
    size_t mapped = align_up(capacity, ps);

#ifdef _WIN32
    void *p = VirtualAlloc(NULL, mapped, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (p == NULL) {
        return NULL;
    }
#else
    void *p = mmap(NULL, mapped, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) {
        return NULL;
    }
#endif

    MemHeap *a = malloc(sizeof(MemHeap));
    if (!a) {
#ifdef _WIN32
        VirtualFree(p, 0, MEM_RELEASE);
#else
        munmap(p, mapped);
#endif
        return NULL;
    }

    a->base = p;
    a->mapped = mapped;
    a->capacity = capacity & ~(ALIGN - 1u);
    a->used = 0;
    a->free_list = malloc(sizeof(FreeNode));
    a->spare = malloc(sizeof(FreeNode));
    if (!a->free_list || !a->spare) {
        free(a->free_list);
        free(a->spare);
#ifdef _WIN32
        VirtualFree(p, 0, MEM_RELEASE);
#else
        munmap(p, mapped);
#endif
        free(a);
        return NULL;
    }
    a->free_list->offset = 0;
    a->free_list->size = a->capacity;
    a->free_list->next = NULL;
    a->spare->next = NULL;
    return a;
}

static void recycle_node(MemHeap *a, FreeNode *node);
static FreeNode *take_node(MemHeap *a);

void arena_destroy(MemHeap *a)
{
    if (!a) {
        return;
    }

    FreeNode *n = a->free_list;
    while (n) {
        FreeNode *next = n->next;
        free(n);
        n = next;
    }
    free(a->spare);
    if (a->base) {
#ifdef _WIN32
        VirtualFree(a->base, 0, MEM_RELEASE);
#else
        munmap(a->base, a->mapped);
#endif
    }
    free(a);
}

void arena_reset(MemHeap *a)
{
    if (!a) {
        return;
    }

    /* One node becomes the whole-arena span. The rest return to the spare
     * or to malloc. Live payloads are not visited. */
    FreeNode *keep = NULL;
    FreeNode *node = a->free_list;
    while (node) {
        FreeNode *next = node->next;
        if (!keep) {
            keep = node;
        } else {
            recycle_node(a, node);
        }
        node = next;
    }

    if (!keep) {
        keep = take_node(a);
        if (!keep) {
            a->free_list = NULL;
            a->used = a->capacity;
            return;
        }
    }

    keep->offset = 0;
    keep->size = a->capacity;
    keep->next = NULL;
    a->free_list = keep;
    a->used = 0;

    if (!a->spare) {
        a->spare = malloc(sizeof(FreeNode));
        if (a->spare) {
            a->spare->next = NULL;
        }
    }
}

/* Payload is aligned to `alignment`. The word just before it stores the
 * distance back to the block header so free can return the padding too. */
static int align_up_ok(const size_t n, const size_t a, size_t *out)
{
    if (a == 0 || n > SIZE_MAX - (a - 1u)) {
        return 0;
    }
    *out = (n + a - 1u) & ~(a - 1u);
    return 1;
}

void *arena_alloc_aligned(MemHeap *a, const size_t n, size_t alignment)
{
    if (!a || n == 0 || alignment == 0 || (alignment & (alignment - 1u)) != 0u) {
        return NULL;
    }

    if (alignment < ALIGN) {
        alignment = ALIGN;
    }

    if (n > SIZE_MAX - HDR_SIZE) {
        return NULL;
    }

    FreeNode *prev = NULL;
    FreeNode *cur = a->free_list;
    while (cur) {
        size_t payload = 0;
        size_t end = 0;
        const int room = cur->offset <= SIZE_MAX - HDR_SIZE
            && align_up_ok(cur->offset + HDR_SIZE, alignment, &payload)
            && payload >= cur->offset + HDR_SIZE
            && payload <= SIZE_MAX - n
            && align_up_ok(payload + n, ALIGN, &end)
            && end >= payload
            && end - cur->offset <= cur->size
            && payload - cur->offset >= HDR_SIZE;

        if (room) {
            const size_t off = cur->offset;
            const size_t span = end - off;
            const size_t back = payload - off;

            if (cur->size == span) {
                if (prev) {
                    prev->next = cur->next;
                } else {
                    a->free_list = cur->next;
                }
                recycle_node(a, cur);
            } else {
                cur->offset += span;
                cur->size -= span;
            }

            Header *h = (Header *)(a->base + off);
            h->size = span;
            *(size_t *)(a->base + payload - sizeof(size_t)) = back;
            a->used += span;
            return a->base + payload;
        }

        prev = cur;
        cur = cur->next;
    }

    return NULL;
}

void *arena_alloc(MemHeap *a, const size_t n)
{
    return arena_alloc_aligned(a, n, ALIGN);
}

/* Keep one unused node so a later free can be recorded without malloc. */
static void recycle_node(MemHeap *a, FreeNode *node)
{
    if (!a->spare) {
        node->next = NULL;
        a->spare = node;
    } else {
        free(node);
    }
}

static FreeNode *take_node(MemHeap *a)
{
    FreeNode *node = a->spare;

    if (node) {
        a->spare = NULL;
        node->next = NULL;
        return node;
    }

    return malloc(sizeof(FreeNode));
}

void arena_free(MemHeap *a, void *p)
{
    if (!a || !p) {
        return;
    }

    /* The word before the payload is the distance back to the block start,
     * including any alignment padding. */
    const size_t back = *(size_t *)((uint8_t *)p - sizeof(size_t));
    if (back < HDR_SIZE || back > (size_t)((uint8_t *)p - a->base)) {
        return;
    }

    Header *h = (Header *)((uint8_t *)p - back);
    const size_t off = (uint8_t *)h - a->base;
    const size_t size = h->size;
    if (off > a->capacity || size > a->capacity - off || size < back) {
        return;
    }

    FreeNode *prev = NULL, *cur = a->free_list;
    while (cur && cur->offset < off) {
        prev = cur;
        cur = cur->next;
    }

    /* Absorb into an existing neighbor so this free needs no new node. */
    if (prev && prev->offset + prev->size == off) {
        prev->size += size;
        if (cur && prev->offset + prev->size == cur->offset) {
            prev->size += cur->size;
            prev->next = cur->next;
            recycle_node(a, cur);
        }
        a->used -= size;
        return;
    }

    if (cur && off + size == cur->offset) {
        cur->offset = off;
        cur->size += size;
        a->used -= size;
        return;
    }

    /* Isolated hole: record it only after a node is in hand. On failure the
     * block stays counted in use and is not dropped from the arena. */
    FreeNode *node = take_node(a);
    if (!node) {
        return;
    }

    node->offset = off;
    node->size = size;
    node->next = cur;

    if (prev) {
        prev->next = node;
    } else {
        a->free_list = node;
    }
    a->used -= size;
}

size_t arena_capacity(const MemHeap *a)
{
    return a ? a->capacity : 0;
}

size_t arena_used(const MemHeap *a)
{
    return a ? a->used : 0;
}
