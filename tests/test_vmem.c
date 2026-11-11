// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#include <bedrock/unitest.h>
#include <bedrock/vmem.h>

#include <string.h>

TEST_CASE(test_vmem_page_size, "[vmem]") {
    const size_t page = vmem_page_size();
    REQUIRE(page >= 4096u);
    REQUIRE((page & (page - 1u)) == 0u);
}

TEST_CASE(test_vmem_align_up, "[vmem]") {
    const size_t page = vmem_page_size();
    REQUIRE(vmem_align_up(0) == 0u);
    REQUIRE(vmem_align_up(1) == page);
    REQUIRE(vmem_align_up(page) == page);
    REQUIRE(vmem_align_up(page + 1u) == page * 2u);
}

TEST_CASE(test_vmem_reject_empty, "[vmem]") {
    VMemoryRegion region = {0};
    REQUIRE(vmem_reserve(NULL, 128) == -1);
    REQUIRE(vmem_reserve(&region, 0) == -1);
    REQUIRE(region.base == NULL);
    REQUIRE(vmem_commit(NULL, 0) == 0);
    REQUIRE(vmem_commit(NULL, 16) == -1);
}

TEST_CASE(test_vmem_reserve_commit_release, "[vmem]") {
    const size_t page = vmem_page_size();
    VMemoryRegion region;
    REQUIRE(vmem_reserve(&region, page * 4u) == 0);
    REQUIRE(region.base != NULL);
    REQUIRE(region.bytes >= page * 4u);
    REQUIRE(region.bytes % page == 0u);

    REQUIRE(vmem_commit(region.base, 1) == 0);
    REQUIRE(vmem_commit(region.base, 1) == 0);
    memset(region.base, 0xAB, page);

    const unsigned char *bytes = region.base;
    REQUIRE(bytes[0] == 0xAB);
    REQUIRE(bytes[page - 1u] == 0xAB);

    vmem_release(&region);
    REQUIRE(region.base == NULL);
    REQUIRE(region.bytes == 0u);
}

TEST_CASE(test_vmem_double_release, "[vmem]") {
    VMemoryRegion region;
    REQUIRE(vmem_reserve(&region, 1) == 0);
    REQUIRE(vmem_commit(region.base, 1) == 0);
    memset(region.base, 0xCD, 1);

    vmem_release(&region);
    vmem_release(&region);
    vmem_release(NULL);
    REQUIRE(region.base == NULL);
    REQUIRE(region.bytes == 0u);
}
