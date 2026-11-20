// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#ifndef _WIN32
#  define _FILE_OFFSET_BITS 64
#  define _POSIX_C_SOURCE 200809L
#  define _DEFAULT_SOURCE
#endif

#include "bedrock/fmap.h"
#include "bedrock/vmem.h"

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  include <errno.h>
#  include <fcntl.h>
#  include <sys/mman.h>
#  include <sys/stat.h>
#  include <unistd.h>
#endif

#include <stdint.h>
#include <stdlib.h>

struct FileMap {
#ifdef _WIN32
    HANDLE file;
#else
    int fd;
#endif
    uint64_t size;
    size_t align;
    void *map_base;
    size_t map_bytes;
    void *fallback;
};

static size_t map_alignment(void)
{
#ifdef _WIN32
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    size_t align = (size_t)info.dwAllocationGranularity;
    if (align == 0) {
        align = (size_t)info.dwPageSize;
    }
    if (align == 0) {
        align = 4096u;
    }
    return align;
#else
    return vmem_page_size();
#endif
}

static void drop_views(FileMap *map)
{
    if (map->map_base != NULL) {
#ifdef _WIN32
        UnmapViewOfFile(map->map_base);
#else
        munmap(map->map_base, map->map_bytes);
#endif
        map->map_base = NULL;
        map->map_bytes = 0;
    }
    free(map->fallback);
    map->fallback = NULL;
}

static void close_file(FileMap *map)
{
#ifdef _WIN32
    if (map->file != NULL && map->file != INVALID_HANDLE_VALUE) {
        CloseHandle(map->file);
        map->file = INVALID_HANDLE_VALUE;
    }
#else
    if (map->fd >= 0) {
        close(map->fd);
        map->fd = -1;
    }
#endif
}

static int read_at(FileMap *map, void *dst, const size_t n, const uint64_t offset)
{
    unsigned char *bytes = dst;
    size_t got = 0;

    while (got < n) {
        if ((uint64_t)got > UINT64_MAX - offset) {
            return -1;
        }
        const uint64_t at = offset + (uint64_t)got;
        size_t chunk = n - got;
        if (chunk > (1u << 30)) {
            chunk = (size_t)1u << 30;
        }

#ifdef _WIN32
        OVERLAPPED overlapped = {0};
        DWORD nread = 0;
        overlapped.Offset = (DWORD)at;
        overlapped.OffsetHigh = (DWORD)(at >> 32);
        if (!ReadFile(map->file, bytes + got, (DWORD)chunk, &nread, &overlapped) || nread == 0) {
            return -1;
        }
        got += (size_t)nread;
#else
        if ((off_t)at < 0 || (uint64_t)(off_t)at != at) {
            return -1;
        }
        const ssize_t nread = pread(map->fd, bytes + got, chunk, (off_t)at);
        if (nread < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (nread == 0) {
            return -1;
        }
        got += (size_t)nread;
#endif
    }
    return 0;
}

/* Map [offset, offset + actual). The returned pointer is offset, not the
 * page-aligned base the OS requires. A zero length is not mapped.
 */
static int map_span(FileMap *map, const uint64_t offset, const size_t actual, void **view_out)
{
    const uint64_t align = map->align;
    const uint64_t aligned = offset & ~(align - 1u);
    const uint64_t delta64 = offset - aligned;

    if (actual == 0 || delta64 > SIZE_MAX || actual > SIZE_MAX - (size_t)delta64) {
        return -1;
    }

    const size_t delta = (size_t)delta64;
    const size_t map_len = delta + actual;
    void *base = NULL;

#ifdef _WIN32
    HANDLE mapping = CreateFileMappingA(map->file, NULL, PAGE_READONLY, 0, 0, NULL);
    if (mapping == NULL) {
        return -1;
    }
    base = MapViewOfFile(mapping, FILE_MAP_READ,
                         (DWORD)(aligned >> 32), (DWORD)aligned, map_len);
    CloseHandle(mapping);
    if (base == NULL) {
        return -1;
    }
#else
    if ((off_t)aligned < 0 || (uint64_t)(off_t)aligned != aligned) {
        return -1;
    }
    base = mmap(NULL, map_len, PROT_READ, MAP_PRIVATE, map->fd, (off_t)aligned);
    if (base == MAP_FAILED) {
        return -1;
    }
#endif

    drop_views(map);
    map->map_base = base;
    map->map_bytes = map_len;
    *view_out = (unsigned char *)base + delta;
    return 0;
}

static int read_span(FileMap *map, const uint64_t offset, const size_t actual, void **view_out)
{
    if (actual == 0) {
        return -1;
    }

    void *buf = malloc(actual);
    if (buf == NULL) {
        return -1;
    }
    if (read_at(map, buf, actual, offset) != 0) {
        free(buf);
        return -1;
    }

    drop_views(map);
    map->fallback = buf;
    *view_out = buf;
    return 0;
}

FileMap *fmap_open(const char *path)
{
    if (path == NULL || path[0] == '\0') {
        return NULL;
    }

    const size_t align = map_alignment();
    if (align == 0 || (align & (align - 1u)) != 0u) {
        return NULL;
    }

#ifdef _WIN32
    const DWORD attr = GetFileAttributesA(path);
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return NULL;
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    LARGE_INTEGER length;
    if (!GetFileSizeEx(file, &length) || length.QuadPart < 0) {
        CloseHandle(file);
        return NULL;
    }

    FileMap *map = calloc(1, sizeof(*map));
    if (map == NULL) {
        CloseHandle(file);
        return NULL;
    }
    map->file = file;
    map->size = (uint64_t)length.QuadPart;
#else
    const int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return NULL;
    }

    struct stat st;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 0) {
        close(fd);
        return NULL;
    }

    FileMap *map = calloc(1, sizeof(*map));
    if (map == NULL) {
        close(fd);
        return NULL;
    }
    map->fd = fd;
    map->size = (uint64_t)st.st_size;
#endif

    map->align = align;
    return map;
}

void fmap_close(FileMap *map)
{
    if (map == NULL) {
        return;
    }
    drop_views(map);
    close_file(map);
    free(map);
}

uint64_t fmap_size(const FileMap *map)
{
    if (map == NULL) {
        return 0;
    }
    return map->size;
}

int fmap_data(FileMap *map, void **out, size_t *size)
{
    if (map == NULL || out == NULL || size == NULL) {
        return -1;
    }
    if (map->size == 0 || map->size > SIZE_MAX) {
        return -1;
    }

    void *view = NULL;
    if (map_span(map, 0, (size_t)map->size, &view) != 0) {
        return -1;
    }
    *out = view;
    *size = (size_t)map->size;
    return 0;
}

int fmap_window(FileMap *map, const uint64_t offset, const size_t length,
                void **out, size_t *actual)
{
    if (map == NULL || out == NULL || actual == NULL) {
        return -1;
    }
    if (offset > map->size) {
        return -1;
    }

    const uint64_t remain = map->size - offset;
    size_t n = length;
    if ((uint64_t)length > remain) {
        n = (size_t)remain;
    }

    if (n == 0) {
        drop_views(map);
        *out = NULL;
        *actual = 0;
        return 0;
    }

    void *view = NULL;
    if (map_span(map, offset, n, &view) != 0) {
        if (read_span(map, offset, n, &view) != 0) {
            return -1;
        }
    }
    *out = view;
    *actual = n;
    return 0;
}
