// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#ifndef _WIN32
#  define _POSIX_C_SOURCE 200809L
#  define _DEFAULT_SOURCE
#endif

#include "bedrock/vmem.h"

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  include <sys/mman.h>
#  include <unistd.h>
#  ifndef MAP_ANONYMOUS
#    define MAP_ANONYMOUS MAP_ANON
#  endif
#endif

#include <stdint.h>

size_t vmem_page_size(void) {
#ifdef _WIN32
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return (info.dwPageSize == 0) ? 4096u : (size_t)info.dwPageSize;
#else
    const long page = sysconf(_SC_PAGESIZE);
    return (page > 0) ? (size_t)page : 4096u;
#endif
}

size_t vmem_align_up(const size_t bytes) {
    if (bytes == 0) {
        return 0;
    }

    const size_t page = vmem_page_size();
    if (page == 0 || (page & (page - 1u)) != 0u) {
        return 0;
    }
    if (bytes > SIZE_MAX - (page - 1u)) {
        return 0;
    }
    return (bytes + page - 1u) & ~(page - 1u);
}

int vmem_reserve(VMemoryRegion *out, const size_t bytes) {
    if (!out || bytes == 0) {
        return -1;
    }

    const size_t mapped = vmem_align_up(bytes);
    if (mapped == 0) {
        return -1;
    }

#ifdef _WIN32
    void *base = VirtualAlloc(NULL, mapped, MEM_RESERVE, PAGE_NOACCESS);
    if (!base) {
        return -1;
    }
#else
    void *base = mmap(NULL, mapped, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED) {
        return -1;
    }
#endif

    out->base = base;
    out->bytes = mapped;
    return 0;
}

int vmem_commit(void *p, const size_t bytes) {
    if (bytes == 0) {
        return 0;
    }
    if (!p) {
        return -1;
    }

    const size_t page = vmem_page_size();
    if (page == 0 || (page & (page - 1u)) != 0u) {
        return -1;
    }

    const uintptr_t addr = (uintptr_t)p;
    if (bytes > UINTPTR_MAX - addr) {
        return -1;
    }

    const uintptr_t start = addr & ~(uintptr_t)(page - 1u);
    const uintptr_t end = (addr + bytes + (page - 1u)) & ~(uintptr_t)(page - 1u);
    const size_t length = (size_t)(end - start);

#ifdef _WIN32
    if (!VirtualAlloc((void *)start, length, MEM_COMMIT, PAGE_READWRITE)) {
        return -1;
    }
#else
    if (mprotect((void *)start, length, PROT_READ | PROT_WRITE) != 0) {
        return -1;
    }
#endif
    return 0;
}

void vmem_release(VMemoryRegion *region) {
    if (!region) {
        return;
    }
    if (region->base && region->bytes > 0) {
#ifdef _WIN32
        VirtualFree(region->base, 0, MEM_RELEASE);
#else
        munmap(region->base, region->bytes);
#endif
    }
    region->base = NULL;
    region->bytes = 0;
}
