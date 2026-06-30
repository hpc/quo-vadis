/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */

/**
 * @file test-map.cc
 */

#include "qvi-common.h" // IWYU pragma: keep
#include "qvi-map.h"

#include "common-test-utils.h"

// Empty map test.
static void
test_1(void)
{
    std::vector<qvi_hwloc_bitmap> src_affinities;
    std::vector<qvi_hwloc_bitmap> dst_affinities;

    qvi_map_config config = {
        src_affinities,
        dst_affinities
    };

    qvi_map_t map;
    int rc = qvi_map_affinity_preserving(
        config, map
    );
    ctu_assert(rc == QV_SUCCESS, "%d != QV_SUCCESS", rc);
    ctu_assert(map.empty(), "!map.empty()");

    qvi_log_info("✓ {} PASSED", __func__);
}

// n == m, no affinity test.
static void
test_2(void)
{
    std::vector<qvi_hwloc_bitmap> src_affinities(3);
    std::vector<qvi_hwloc_bitmap> dst_affinities(3);

    qvi_map_config config = {
        src_affinities,
        dst_affinities
    };

    qvi_map_t map;
    int rc = qvi_map_affinity_preserving(
        config, map
    );
    ctu_assert(rc == QV_SUCCESS, "%d != QV_SUCCESS", rc);

    qvi_map_t expected = {
        {0, {0}},
        {1, {1}},
        {2, {2}}
    };
    ctu_assert(map == expected, "unexpected result");

    qvi_log_info("✓ {} PASSED", __func__);
}

// n < m, no affinity test.
static void
test_3(void)
{
    std::vector<qvi_hwloc_bitmap> src_affinities(3);
    std::vector<qvi_hwloc_bitmap> dst_affinities(6);

    qvi_map_config config = {
        src_affinities,
        dst_affinities
    };

    qvi_map_t map;
    int rc = qvi_map_affinity_preserving(
        config, map
    );
    ctu_assert(rc == QV_SUCCESS, "%d != QV_SUCCESS", rc);

    qvi_map_t expected = {
        {0, {0, 3}},
        {1, {1, 4}},
        {2, {2, 5}}
    };
    ctu_assert(map == expected, "unexpected result");

    qvi_log_info("✓ {} PASSED", __func__);
}

// n == m, with matching src/dst affinity test.
static void
test_4(void)
{
    std::vector<qvi_hwloc_bitmap> src_affinities = ctu_bitmap_split_pus(8, 4);
    std::vector<qvi_hwloc_bitmap> dst_affinities = src_affinities;

    qvi_map_config config = {
        src_affinities,
        dst_affinities
    };

    qvi_map_t map;
    int rc = qvi_map_affinity_preserving(
        config, map
    );
    ctu_assert(rc == QV_SUCCESS, "%d != QV_SUCCESS", rc);

    qvi_map_t expected = {
        {0, {0}},
        {1, {1}},
        {2, {2}},
        {3, {3}}
    };
    ctu_assert(map == expected, "unexpected result");

    qvi_log_info("✓ {} PASSED", __func__);
}

// n == m, with different src/dst affinity test.
static void
test_5(void)
{
    std::vector<qvi_hwloc_bitmap> src_affinities = ctu_bitmap_split_pus(8, 4);
    std::vector<qvi_hwloc_bitmap> dst_affinities = ctu_bitmap_split_pus(8, 2);

    qvi_map_config config = {
        src_affinities,
        dst_affinities
    };

    qvi_map_t map;
    int rc = qvi_map_affinity_preserving(
        config, map
    );
    ctu_assert(rc == QV_SUCCESS, "%d != QV_SUCCESS", rc);

    qvi_map_t expected = {
        {0, {0}},
        {1, {0}},
        {2, {1}},
        {3, {1}}
    };
    ctu_assert(map == expected, "unexpected result");

    qvi_log_info("✓ {} PASSED", __func__);
}

int
main(void)
{
    printf("\n# Starting map test\n");

    test_1();
    test_2();
    test_3();
    test_4();
    test_5();

    qvi_log_info("✓ All tests PASSED");
    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
