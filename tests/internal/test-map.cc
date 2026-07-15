/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */

/**
 * @file test-map.cc
 */

#include "qvi-common.h" // IWYU pragma: keep
#include "qvi-map.h"

#include "common-test-utils.h"

static qvi_hwloc_bitmap
ctu_bitmap_gen_pus(
    size_t i,
    size_t n
) {
    qvi_hwloc_bitmap result;
    for (size_t ii = i; ii < n; ++ii) {
        hwloc_bitmap_set(result.data(), ii);
    }
    return result;
}

static inline std::vector<qvi_hwloc_bitmap>
ctu_bitmap_split_pus(
    size_t npus,
    size_t npieces
) {
    std::vector<qvi_hwloc_bitmap> result;
    const size_t base_chunk_size = npus / npieces;
    const size_t remainder = npus % npieces;
    size_t current_pos = 0;

    for (size_t i = 0; i < npieces; ++i) {
        size_t current_chunk_size = base_chunk_size + (i < remainder ? 1 : 0);
        result.emplace_back(
            ctu_bitmap_gen_pus(current_pos, current_pos + current_chunk_size)
        );
        current_pos += current_chunk_size;
    }
    return result;
}

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

// n > m, no affinity test.
static void
test_3(void)
{
    std::vector<qvi_hwloc_bitmap> src_affinities(6);
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
        {1, {0}},
        {2, {1}},
        {3, {1}},
        {4, {2}},
        {5, {2}}
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

// n > m, with bipartite affinity.
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

// n > m, with reversed bipartite affinity.
static void
test_6(void)
{
    std::vector<qvi_hwloc_bitmap> src_affinities = ctu_bitmap_split_pus(8, 4);
    std::reverse(src_affinities.begin(), src_affinities.end());
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
        {0, {1}},
        {1, {1}},
        {2, {0}},
        {3, {0}}
    };
    ctu_assert(map == expected, "unexpected result");

    qvi_log_info("✓ {} PASSED", __func__);
}

// n > m, all srcs prefer same destination.
static void
test_7(void)
{
    std::vector<qvi_hwloc_bitmap> src_affinities = ctu_bitmap_split_pus(6, 6);
    std::vector<qvi_hwloc_bitmap> dst_affinities;
    for (size_t i = 0; i < 2; ++i) {
        dst_affinities.emplace_back(ctu_bitmap_gen_pus(0, 6));
    }

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
        {2, {0}},
        {3, {1}},
        {4, {1}},
        {5, {1}}
    };
    ctu_assert(map == expected, "unexpected result");

    qvi_log_info("✓ {} PASSED", __func__);
}

// n > m, non-overlapping src-->dst preferences.
static void
test_8(void)
{
    std::vector<qvi_hwloc_bitmap> src_affinities = ctu_bitmap_split_pus(3, 3);
    std::vector<qvi_hwloc_bitmap> dst_affinities;
    // dst 0 has no preference.
    dst_affinities.emplace_back(qvi_hwloc_bitmap());
    // dst 1 has affinity to all PUs.
    dst_affinities.emplace_back(ctu_bitmap_gen_pus(0, 3));

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
        {0, {1}},
        {1, {1}},
        {2, {0}}
    };
    ctu_assert(map == expected, "unexpected result");

    qvi_log_info("✓ {} PASSED", __func__);
}

// n == m, first 2 have no preference, second 2 have same preference.
static void
test_9(void)
{
    std::vector<qvi_hwloc_bitmap> src_affinities = ctu_bitmap_split_pus(4, 4);
    std::vector<qvi_hwloc_bitmap> dst_affinities;
    // dst 0 has no preference.
    dst_affinities.emplace_back(qvi_hwloc_bitmap());
    // dst 1 has no preference.
    dst_affinities.emplace_back(qvi_hwloc_bitmap());
    // dst 2 has affinity to last 2 PUs.
    dst_affinities.emplace_back(ctu_bitmap_gen_pus(2, 4));
    // dst 3 has affinity to last 2 PUs.
    dst_affinities.emplace_back(ctu_bitmap_gen_pus(2, 4));

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

// n == 2m, map packed
static void
test_10(void)
{
    size_t nsrc = 10;
    size_t ndst = 5;

    qvi_map_config config = {
        nsrc,
        ndst
    };

    qvi_map_t map;
    int rc = qvi_map_packed(
        config, map
    );
    ctu_assert(rc == QV_SUCCESS, "%d != QV_SUCCESS", rc);

    qvi_map_t expected = {
        {0, {0}},
        {1, {0}},
        {2, {1}},
        {3, {1}},
        {4, {2}},
        {5, {2}},
        {6, {3}},
        {7, {3}},
        {8, {4}},
        {9, {4}}
    };
    ctu_assert(map == expected, "unexpected result");

    qvi_log_info("✓ {} PASSED", __func__);
}

// n < m, map odd packed
static void
test_11(void)
{
    size_t nsrc = 5;
    size_t ndst = 7;

    qvi_map_config config = {
        nsrc,
        ndst
    };

    qvi_map_t map;
    int rc = qvi_map_packed(
        config, map
    );
    ctu_assert(rc == QV_SUCCESS, "%d != QV_SUCCESS", rc);

    qvi_map_t expected = {
        {0, {0, 1}},
        {1, {2, 3}},
        {2, {4}},
        {3, {5}},
        {4, {6}}
    };
    ctu_assert(map == expected, "unexpected result");

    qvi_log_info("✓ {} PASSED", __func__);
}

// n == 2m, map spread
static void
test_12(void)
{
    size_t nsrc = 10;
    size_t ndst = 5;

    qvi_map_config config = {
        nsrc,
        ndst
    };

    qvi_map_t map;
    int rc = qvi_map_spread(
        config, map
    );
    ctu_assert(rc == QV_SUCCESS, "%d != QV_SUCCESS", rc);

    qvi_map_t expected = {
        {0, {0}},
        {1, {1}},
        {2, {2}},
        {3, {3}},
        {4, {4}},
        {5, {0}},
        {6, {1}},
        {7, {2}},
        {8, {3}},
        {9, {4}}
    };
    ctu_assert(map == expected, "unexpected result");

    qvi_log_info("✓ {} PASSED", __func__);
}

// n < m, map odd spread
static void
test_13(void)
{
    size_t nsrc = 5;
    size_t ndst = 7;

    qvi_map_config config = {
        nsrc,
        ndst
    };

    qvi_map_t map;
    int rc = qvi_map_spread(
        config, map
    );
    ctu_assert(rc == QV_SUCCESS, "%d != QV_SUCCESS", rc);

    qvi_map_t expected = {
        {0, {0, 5}},
        {1, {1, 6}},
        {2, {2}},
        {3, {3}},
        {4, {4}}
    };
    ctu_assert(map == expected, "unexpected result");

    qvi_log_info("✓ {} PASSED", __func__);
}

// Test qvi_map_colors.
static void
test_14(void)
{
    std::vector<int> src_colors = {0, 2, 3, 1};

    qvi_map_config config = {
        src_colors,
    };

    qvi_map_t map;
    int rc = qvi_map_colors(
        config, map
    );
    ctu_assert(rc == QV_SUCCESS, "%d != QV_SUCCESS", rc);

    qvi_map_t expected = {
        {0, {0}},
        {1, {2}},
        {2, {3}},
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
    test_6();
    test_7();
    test_8();
    test_9();
    test_10();
    test_11();
    test_12();
    test_13();
    test_14();

    qvi_log_info("✓ All tests PASSED");
    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
