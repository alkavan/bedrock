// SPDX-FileCopyrightText: 2025-2026 Igal Alkon
// SPDX-FileCopyrightText: 2026 ALKONTEK <git@alkontek.com>
// SPDX-License-Identifier: BSD-3-Clause

#include <bedrock/fmap.h>
#include <bedrock/unitest.h>
#include <bedrock/vmem.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int write_file(const char *path, const void *data, size_t n)
{
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        return -1;
    }

    const size_t wrote = (n == 0) ? 0 : fwrite(data, 1, n, file);
    const int bad = ferror(file) || wrote != n;
    const int closed = fclose(file);
    return (bad || closed != 0) ? -1 : 0;
}

TEST_CASE(test_fmap_open_rejects, "[fmap]") {
    REQUIRE(fmap_open(NULL) == NULL);
    REQUIRE(fmap_open("") == NULL);
    REQUIRE(fmap_open("bedrock_fmap_missing.bin") == NULL);
    REQUIRE(fmap_open(".") == NULL);
    REQUIRE(fmap_size(NULL) == 0);

    fmap_close(NULL);
    fmap_close(NULL);
}

TEST_CASE(test_fmap_empty, "[fmap]") {
    const char *path = "bedrock_fmap_empty.bin";
    remove(path);
    REQUIRE(write_file(path, "", 0) == 0);

    FileMap *map = fmap_open(path);
    REQUIRE(map != NULL);
    REQUIRE(fmap_size(map) == 0);

    char sentinel = 0;
    void *out = &sentinel;
    size_t n = 7;
    REQUIRE(fmap_data(map, &out, &n) == -1);
    REQUIRE(out == &sentinel);
    REQUIRE(n == 7);

    REQUIRE(fmap_window(map, 0, 0, &out, &n) == 0);
    REQUIRE(out == NULL);
    REQUIRE(n == 0);

    out = &sentinel;
    n = 7;
    REQUIRE(fmap_window(map, 0, 4, &out, &n) == 0);
    REQUIRE(out == NULL);
    REQUIRE(n == 0);

    out = &sentinel;
    n = 7;
    REQUIRE(fmap_window(map, 1, 0, &out, &n) == -1);
    REQUIRE(out == &sentinel);
    REQUIRE(n == 7);

    fmap_close(map);
    remove(path);
}

TEST_CASE(test_fmap_small_file, "[fmap]") {
    const char *path = "bedrock_fmap_small.bin";
    const char expected[] = "abcdefghijklmnopqrstuvwxyz";
    const size_t len = sizeof expected - 1;
    remove(path);
    REQUIRE(write_file(path, expected, len) == 0);

    FileMap *map = fmap_open(path);
    REQUIRE(map != NULL);
    REQUIRE(fmap_size(map) == len);

    void *data = NULL;
    size_t size = 0;
    REQUIRE(fmap_data(map, &data, &size) == 0);
    REQUIRE(data != NULL);
    REQUIRE(size == len);
    REQUIRE(memcmp(data, expected, len) == 0);

    void *window = NULL;
    size_t actual = 0;
    REQUIRE(fmap_window(map, 0, len, &window, &actual) == 0);
    REQUIRE(window != NULL);
    REQUIRE(actual == len);
    REQUIRE(memcmp(window, expected, len) == 0);

    REQUIRE(fmap_window(map, 3, 4, &window, &actual) == 0);
    REQUIRE(actual == 4);
    REQUIRE(memcmp(window, "defg", 4) == 0);

    REQUIRE(fmap_window(map, 4, 100, &window, &actual) == 0);
    REQUIRE(actual == len - 4);
    REQUIRE(memcmp(window, expected + 4, actual) == 0);

    char sentinel = 0;
    void *kept = &sentinel;
    size_t kept_n = 99;
    REQUIRE(fmap_window(map, len + 1, 4, &kept, &kept_n) == -1);
    REQUIRE(kept == &sentinel);
    REQUIRE(kept_n == 99);

    REQUIRE(fmap_window(map, len, 4, &window, &actual) == 0);
    REQUIRE(window == NULL);
    REQUIRE(actual == 0);

    REQUIRE(fmap_window(NULL, 0, 1, &kept, &kept_n) == -1);
    REQUIRE(fmap_window(map, 0, 1, NULL, &kept_n) == -1);
    REQUIRE(fmap_window(map, 0, 1, &kept, NULL) == -1);
    REQUIRE(fmap_data(NULL, &kept, &kept_n) == -1);
    REQUIRE(fmap_data(map, NULL, &kept_n) == -1);
    REQUIRE(fmap_data(map, &kept, NULL) == -1);
    REQUIRE(kept == &sentinel);
    REQUIRE(kept_n == 99);

    fmap_close(map);
    remove(path);
}

TEST_CASE(test_fmap_pages, "[fmap]") {
    const char *path = "bedrock_fmap_pages.bin";
    const size_t page = vmem_page_size();
    const size_t pages = 4;
    unsigned char *buf = malloc(page * pages);
    REQUIRE(buf != NULL);
    for (size_t i = 0; i < pages; ++i) {
        memset(buf + i * page, (int)(i + 1), page);
    }

    remove(path);
    const int wrote = write_file(path, buf, page * pages);
    free(buf);
    REQUIRE(wrote == 0);

    FileMap *map = fmap_open(path);
    REQUIRE(map != NULL);
    REQUIRE(fmap_size(map) == (uint64_t)page * pages);

    for (size_t i = 0; i < pages; ++i) {
        void *view = NULL;
        size_t actual = 0;
        REQUIRE(fmap_window(map, (uint64_t)i * page, page, &view, &actual) == 0);
        REQUIRE(view != NULL);
        REQUIRE(actual == page);
        const unsigned char *bytes = view;
        REQUIRE(bytes[0] == (unsigned char)(i + 1));
        REQUIRE(bytes[page - 1] == (unsigned char)(i + 1));
    }

    void *cross = NULL;
    size_t actual = 0;
    REQUIRE(fmap_window(map, page - 3, 6, &cross, &actual) == 0);
    REQUIRE(actual == 6);
    const unsigned char *bytes = cross;
    REQUIRE(bytes[0] == 1);
    REQUIRE(bytes[2] == 1);
    REQUIRE(bytes[3] == 2);
    REQUIRE(bytes[5] == 2);

    fmap_close(map);
    remove(path);
}
