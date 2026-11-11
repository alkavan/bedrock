// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#ifndef BEDROCK_VMEM_H
#define BEDROCK_VMEM_H

#include <stddef.h>

/**
 * @brief A reserved virtual-address region.
 *
 * `bytes` is the reserved length, rounded up to a whole number of pages.
 * The pages are not accessible until `vmem_commit`.
 */
typedef struct VMemoryRegion {
    void *base;
    size_t bytes;
} VMemoryRegion;

/**
 * @brief System page size in bytes.
 * @return A power of two, or 4096 if the OS does not report a page size.
 */
size_t vmem_page_size(void);

/**
 * @brief Round a byte count up to a multiple of the page size.
 * @param bytes Length to round. Zero stays zero.
 * @return Rounded length, or 0 if `bytes` is 0 or the rounded value would overflow.
 */
size_t vmem_align_up(size_t bytes);

/**
 * @brief Reserve inaccessible virtual address space.
 *
 * On success the region is page-aligned and not yet readable or writable.
 * Physical memory is charged only when the pages are committed.
 *
 * @param out Receives the reservation. Unchanged on failure.
 * @param bytes Requested length. Rounded up to the page size.
 * @return 0 on success, or -1 if `out` is NULL, `bytes` is 0, or the OS refuses.
 */
int vmem_reserve(VMemoryRegion *out, size_t bytes);

/**
 * @brief Make the pages covering `[p, p + bytes)` readable and writable.
 *
 * The range is expanded to whole pages. Committing the same pages again is
 * success. `bytes == 0` is success and does not touch memory.
 *
 * @param p Address inside a region from `vmem_reserve`.
 * @param bytes Number of bytes that must become accessible.
 * @return 0 on success, or -1 if `p` is NULL and `bytes` is non-zero, or the OS refuses.
 */
int vmem_commit(void *p, size_t bytes);

/**
 * @brief Release a whole reservation and zero the struct.
 *
 * NULL and already-released regions are ignored.
 *
 * @param region Region returned by `vmem_reserve`.
 */
void vmem_release(VMemoryRegion *region);

#endif /* BEDROCK_VMEM_H */
