/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2026 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file test-bbuff.cc
 */

#include "qvi-common.h" // IWYU pragma: keep
#include "qvi-bbuff.h"

#include "common-test-utils.h"

// Newly constructed buffer is empty but has a valid backing store.
static void
test_1(void)
{
    qvi_bbuff bbuff;

    ctu_assert(bbuff.size() == 0, "new buffer size != 0");
    ctu_assert(bbuff.data() != nullptr, "new buffer data() == nullptr");
    ctu_assert(bbuff.cdata() != nullptr, "new buffer cdata() == nullptr");

    qvi_log_info("✓ {} PASSED", __func__);
}

// Basic append updates size and stores data correctly.
static void
test_2(void)
{
    qvi_bbuff bbuff;

    const char *msg = "hello";
    const size_t len = strlen(msg) + 1;

    const int rc = bbuff.append(msg, len);
    ctu_assert(rc == QV_SUCCESS, "append failed");
    ctu_assert(bbuff.size() == len, "size mismatch after append");
    ctu_assert(
        memcmp(bbuff.cdata(), msg, len) == 0,
        "data mismatch after append"
    );

    qvi_log_info("✓ {} PASSED", __func__);
}

// Multiple appends accumulate contiguously.
static void
test_3(void)
{
    qvi_bbuff bbuff;

    const char *a = "foo";
    const char *b = "bar";
    const char *c = "baz";
    const size_t la = strlen(a);
    const size_t lb = strlen(b);
    const size_t lc = strlen(c);

    ctu_assert(bbuff.append(a, la) == QV_SUCCESS, "append a failed");
    ctu_assert(bbuff.append(b, lb) == QV_SUCCESS, "append b failed");
    ctu_assert(bbuff.append(c, lc) == QV_SUCCESS, "append c failed");

    ctu_assert(bbuff.size() == la + lb + lc, "size mismatch");
    ctu_assert(
        memcmp(bbuff.cdata(), "foobarbaz", la + lb + lc) == 0,
        "concatenated data mismatch"
    );

    qvi_log_info("✓ {} PASSED", __func__);
}

// Appending zero bytes is a no-op with respect to size.
static void
test_4(void)
{
    qvi_bbuff bbuff;

    const char *msg = "data";
    const size_t len = strlen(msg);
    ctu_assert(bbuff.append(msg, len) == QV_SUCCESS, "append failed");

    ctu_assert(bbuff.append(msg, 0) == QV_SUCCESS, "zero-length append failed");
    ctu_assert(bbuff.size() == len, "size changed after zero-length append");

    qvi_log_info("✓ {} PASSED", __func__);
}

// Append that forces growth beyond the minimum capacity preserves all data.
static void
test_5(void)
{
    qvi_bbuff bbuff;

    // s_min_growth is 256; append more than that to trigger a resize.
    const size_t n = 1024;
    std::vector<byte_t> src(n);
    for (size_t i = 0; i < n; ++i) {
        src[i] = (byte_t)(i & 0xff);
    }

    ctu_assert(bbuff.append(src.data(), n) == QV_SUCCESS, "large append failed");
    ctu_assert(bbuff.size() == n, "size mismatch after large append");
    ctu_assert(
        memcmp(bbuff.cdata(), src.data(), n) == 0,
        "data mismatch after large append"
    );

    // Append again to force another growth.
    ctu_assert(bbuff.append(src.data(), n) == QV_SUCCESS, "second append failed");
    ctu_assert(bbuff.size() == 2 * n, "size mismatch after second append");
    const byte_t *raw = (const byte_t *)bbuff.cdata();
    ctu_assert(
        memcmp(raw, src.data(), n) == 0,
        "first chunk corrupted after growth"
    );
    ctu_assert(
        memcmp(raw + n, src.data(), n) == 0,
        "second chunk mismatch after growth"
    );

    qvi_log_info("✓ {} PASSED", __func__);
}

// Copy constructor produces an independent, equal copy.
static void
test_6(void)
{
    qvi_bbuff src;

    const char *msg = "copy-me";
    const size_t len = strlen(msg) + 1;
    ctu_assert(src.append(msg, len) == QV_SUCCESS, "append failed");

    qvi_bbuff dst(src);
    ctu_assert(dst.size() == src.size(), "copy size mismatch");
    ctu_assert(
        memcmp(dst.cdata(), src.cdata(), len) == 0,
        "copy data mismatch"
    );
    // Backing stores must be distinct.
    ctu_assert(dst.cdata() != src.cdata(), "copy shares backing store");

    // Mutating the copy must not affect the source.
    ctu_assert(dst.append("x", 1) == QV_SUCCESS, "append to copy failed");
    ctu_assert(src.size() == len, "source changed after mutating copy");

    qvi_log_info("✓ {} PASSED", __func__);
}

// Copy constructing from an empty buffer works.
static void
test_7(void)
{
    qvi_bbuff src;
    qvi_bbuff dst(src);

    ctu_assert(dst.size() == 0, "empty copy size != 0");
    ctu_assert(dst.data() != nullptr, "empty copy data() == nullptr");

    qvi_log_info("✓ {} PASSED", __func__);
}

// Assignment operator produces an independent, equal copy.
static void
test_8(void)
{
    qvi_bbuff src;

    const char *msg = "assign-me";
    const size_t len = strlen(msg) + 1;
    ctu_assert(src.append(msg, len) == QV_SUCCESS, "append failed");

    qvi_bbuff dst;
    // Give dst some pre-existing content to ensure it is reset on assign.
    ctu_assert(dst.append("stale-data", 10) == QV_SUCCESS, "pre-append failed");

    dst = src;
    ctu_assert(dst.size() == src.size(), "assign size mismatch");
    ctu_assert(
        memcmp(dst.cdata(), src.cdata(), len) == 0,
        "assign data mismatch"
    );
    ctu_assert(dst.cdata() != src.cdata(), "assign shares backing store");

    qvi_log_info("✓ {} PASSED", __func__);
}

// pack/unpack round-trip of scalar values.
static void
test_9(void)
{
    qvi_bbuff bbuff;

    const int in_i = 42;
    const double in_d = 3.14159;

    ctu_assert(bbuff.pack(in_i, in_d) == QV_SUCCESS, "pack failed");
    ctu_assert(bbuff.size() > 0, "pack produced no data");

    int out_i = 0;
    double out_d = 0.0;
    ctu_assert(
        qvi_bbuff::unpack(bbuff.data(), out_i, out_d) == QV_SUCCESS,
        "unpack failed"
    );
    ctu_assert(out_i == in_i, "unpacked int mismatch");
    ctu_assert(out_d == in_d, "unpacked double mismatch");

    qvi_log_info("✓ {} PASSED", __func__);
}

// pack/unpack round-trip of a string and a vector.
static void
test_10(void)
{
    qvi_bbuff bbuff;

    const std::string in_s = "the quick brown fox";
    const std::vector<int> in_v = {1, 2, 3, 4, 5};

    ctu_assert(bbuff.pack(in_s, in_v) == QV_SUCCESS, "pack failed");

    std::string out_s;
    std::vector<int> out_v;
    ctu_assert(
        qvi_bbuff::unpack(bbuff.data(), out_s, out_v) == QV_SUCCESS,
        "unpack failed"
    );
    ctu_assert(out_s == in_s, "unpacked string mismatch");
    ctu_assert(out_v == in_v, "unpacked vector mismatch");

    qvi_log_info("✓ {} PASSED", __func__);
}

int
main(void)
{
    qvi_log_info("\n# Starting bbuff test");

    test_1();
    test_2();
    test_3();
    test_4();
    test_5();
    test_6();
    test_7();
    test_8();
    test_9();
    test_10();

    qvi_log_info("✓ All tests PASSED");
    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
