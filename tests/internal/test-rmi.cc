/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */

/**
 * @file test-rmi.cc
 *
 * Unit tests for the Resource Management and Inquiry (RMI) client/server
 * machinery in qvi-rmi.cc. The tests keep the -s/-c/-cc process harness used
 * by the CMake test driver: a server process is spawned in the background and
 * one or more client processes exercise the client-side RPC surface. The final
 * client (-cc) sends the shutdown message so the server exits cleanly.
 *
 * The client-side checks compare RMI results against the client's own local
 * hwloc instance. Because the server exports (and the client loads) the same
 * host topology, the two should agree; disagreements typically point to bugs
 * in the RPC pack/unpack path, argument ordering, or reply handling in
 * qvi-rmi.cc.
 */

#include "quo-vadis.h"
#include "qvi-utils.h"
#include "qvi-hwloc.h"
#include "qvi-hwpool.h"
#include "qvi-rmi.h"

#include "common-test-utils.h"

static int
server(
    const char *url
) {
    printf("# [%d] Starting Server (%s)\n", getpid(), url);

    char const *ers = nullptr;
    qvi_hwloc hwloc;
    qvi_rmi_config config;
    qvi_rmi_server server;

    int rc = hwloc.topology_init(QVI_HWLOC_FLAG_TOPO_FULL);
    if (rc != QV_SUCCESS) {
        ers = "hwloc.topology_init() failed";
        goto out;
    }

    rc = hwloc.topology_load();
    if (rc != QV_SUCCESS) {
        ers = "hwloc.topology_load() failed";
        goto out;
    }

    config.url = std::string(url);

    rc = hwloc.topology_export(qvi_tmpdir());
    if (rc != QV_SUCCESS) {
        ers = "hwloc.topology_export() failed";
        goto out;
    }

    rc = server.configure(config);
    if (rc != QV_SUCCESS) {
        ers = "server.configure() failed";
        goto out;
    }

    printf("# [%d] Server Started\n", getpid());
    // Note: start() blocks in the main service loop until a shutdown message is
    // received, so the "started" message above must be emitted beforehand.
    rc = server.start();
    if (rc != QV_SUCCESS) {
        ers = "server.start() failed";
        goto out;
    }
out:
    if (ers) {
        fprintf(stderr, "\n%s (rc=%d, %s)\n", ers, rc, qv_strerr(rc));
        return 1;
    }
    return 0;
}

static int
get_portno(
    char *url,
    int *portno
) {
    char *pos = strrchr(url, ':');
    if (!pos) return 1;

    char *ports = pos + 1;
    *portno = atoi(ports);
    return 0;
}

/**
 * Verifies get_cpubind returns the same cpuset that the client's local hwloc
 * reports for the same PID. Exercises the reply-side unpack of a bitmap.
 */
static void
check_get_cpubind(
    qvi_rmi_client *client,
    qvi_hwloc &lhwloc,
    pid_t who
) {
    qvi_hwloc_bitmap rmi_bitmap;
    int rc = client->get_cpubind(who, rmi_bitmap);
    ctu_assert(rc == QV_SUCCESS, "get_cpubind() failed (rc=%s)", qv_strerr(rc));

    qvi_hwloc_bitmap local_bitmap;
    rc = lhwloc.task_get_cpubind(who, local_bitmap);
    ctu_assert(
        rc == QV_SUCCESS,
        "local task_get_cpubind() failed (rc=%s)", qv_strerr(rc)
    );

    ctu_assert(
        rmi_bitmap == local_bitmap,
        "get_cpubind() mismatch: rmi=%s local=%s",
        qvi_hwloc::bitmap_string(rmi_bitmap).c_str(),
        qvi_hwloc::bitmap_string(local_bitmap).c_str()
    );

    printf(
        "# [%d] ✓ get_cpubind = %s\n",
        who, qvi_hwloc::bitmap_string(rmi_bitmap).c_str()
    );
}

/**
 * Round-trips set_cpubind followed by get_cpubind. Setting the binding to the
 * current binding must be a no-op that the subsequent query reflects.
 */
static void
check_set_cpubind(
    qvi_rmi_client *client,
    pid_t who
) {
    qvi_hwloc_bitmap before;
    int rc = client->get_cpubind(who, before);
    ctu_assert(rc == QV_SUCCESS, "get_cpubind() failed (rc=%s)", qv_strerr(rc));

    // Re-apply the existing binding. This should always be legal.
    rc = client->set_cpubind(who, before);
    ctu_assert(rc == QV_SUCCESS, "set_cpubind() failed (rc=%s)", qv_strerr(rc));

    qvi_hwloc_bitmap after;
    rc = client->get_cpubind(who, after);
    ctu_assert(rc == QV_SUCCESS, "get_cpubind() failed (rc=%s)", qv_strerr(rc));

    ctu_assert(
        before == after,
        "set_cpubind() perturbed binding: before=%s after=%s",
        qvi_hwloc::bitmap_string(before).c_str(),
        qvi_hwloc::bitmap_string(after).c_str()
    );

    printf("# [%d] ✓ set_cpubind round-trip stable\n", who);
}

/**
 * Verifies get_obj_depth agrees with the client's local hwloc for a range of
 * object types. Exercises the reply-side unpack of a plain int and correct
 * request argument ordering (flags, type).
 */
static void
check_get_obj_depth(
    qvi_rmi_client *client,
    qvi_hwloc &lhwloc
) {
    for (size_t i = 0; i < ctu_hw_obj_name_to_type_tab_size; ++i) {
        const qv_hw_obj_type_t type = ctu_hw_obj_name_to_type_tab[i].type;

        int rmi_depth = -424242;
        int rc = client->get_obj_depth(type, rmi_depth);
        ctu_assert(
            rc == QV_SUCCESS,
            "get_obj_depth(%s) failed (rc=%s)",
            ctu_hw_obj_name_to_type_tab[i].name, qv_strerr(rc)
        );
        // Sentinel must have been overwritten by the reply.
        ctu_assert(
            rmi_depth != -424242,
            "get_obj_depth(%s) did not populate depth",
            ctu_hw_obj_name_to_type_tab[i].name
        );

        int local_depth = 0;
        rc = lhwloc.obj_type_depth(type, &local_depth);
        ctu_assert(
            rc == QV_SUCCESS,
            "local obj_type_depth(%s) failed (rc=%s)",
            ctu_hw_obj_name_to_type_tab[i].name, qv_strerr(rc)
        );

        ctu_assert(
            rmi_depth == local_depth,
            "get_obj_depth(%s) mismatch: rmi=%d local=%d",
            ctu_hw_obj_name_to_type_tab[i].name, rmi_depth, local_depth
        );

        printf(
            "# ✓ get_obj_depth(%s) = %d\n",
            ctu_hw_obj_name_to_type_tab[i].name, rmi_depth
        );
    }
}

/**
 * Verifies get_nobjs_in_cpuset over the whole-machine cpuset agrees with the
 * client's local hwloc. Exercises reply-side unpack of a size_t and the
 * request-side pack of a bitmap.
 */
static void
check_get_nobjs_in_cpuset(
    qvi_rmi_client *client,
    qvi_hwloc &lhwloc
) {
    const qvi_hwloc_bitmap machine(lhwloc.topology_get_cpuset());

    const qv_hw_obj_type_t types[] = {
        QV_HW_OBJ_PACKAGE, QV_HW_OBJ_CORE, QV_HW_OBJ_PU
    };
    for (const auto type : types) {
        size_t rmi_nobjs = SIZE_MAX;
        int rc = client->get_nobjs_in_cpuset(type, machine, rmi_nobjs);
        ctu_assert(
            rc == QV_SUCCESS,
            "get_nobjs_in_cpuset(%s) failed (rc=%s)",
            ctu_obj_name(type), qv_strerr(rc)
        );

        size_t local_nobjs = 0;
        rc = lhwloc.get_nobjs_in_cpuset(type, machine.cdata(), local_nobjs);
        ctu_assert(
            rc == QV_SUCCESS,
            "local get_nobjs_in_cpuset(%s) failed (rc=%s)",
            ctu_obj_name(type), qv_strerr(rc)
        );

        ctu_assert(
            rmi_nobjs == local_nobjs,
            "get_nobjs_in_cpuset(%s) mismatch: rmi=%zu local=%zu",
            ctu_obj_name(type), rmi_nobjs, local_nobjs
        );

        printf(
            "# ✓ get_nobjs_in_cpuset(%s) = %zu\n",
            ctu_obj_name(type), rmi_nobjs
        );
    }
}

/**
 * Verifies get_cpuset_for_nobjs returns a sane, non-empty subset cpuset when
 * requesting a single object from the whole-machine cpuset.
 */
static void
check_get_cpuset_for_nobjs(
    qvi_rmi_client *client,
    qvi_hwloc &lhwloc
) {
    const qvi_hwloc_bitmap machine(lhwloc.topology_get_cpuset());

    qvi_hwloc_bitmap result;
    int rc = client->get_cpuset_for_nobjs(machine, QV_HW_OBJ_CORE, 1, result);
    ctu_assert(
        rc == QV_SUCCESS,
        "get_cpuset_for_nobjs() failed (rc=%s)", qv_strerr(rc)
    );

    // The result should be a non-empty subset of the machine cpuset.
    ctu_assert(
        !hwloc_bitmap_iszero(result.cdata()),
        "get_cpuset_for_nobjs() returned an empty cpuset"
    );
    ctu_assert(
        hwloc_bitmap_isincluded(result.cdata(), machine.cdata()),
        "get_cpuset_for_nobjs() result=%s not included in machine=%s",
        qvi_hwloc::bitmap_string(result).c_str(),
        qvi_hwloc::bitmap_string(machine).c_str()
    );

    printf(
        "# ✓ get_cpuset_for_nobjs(CORE, 1) = %s\n",
        qvi_hwloc::bitmap_string(result).c_str()
    );
}

/**
 * Verifies get_device_in_cpuset. When GPU devices are present, the RMI result
 * must match the client's local hwloc lookup for the same device. When no GPUs
 * are present, requesting device 0 must fail cleanly rather than succeed with
 * garbage. Exercises reply-side unpack of a std::string and request-side pack
 * of a mixed argument list (obj, index, bitmap, id-type).
 */
static void
check_get_device_in_cpuset(
    qvi_rmi_client *client,
    qvi_hwloc &lhwloc
) {
    const qvi_hwloc_bitmap machine(lhwloc.topology_get_cpuset());

    size_t ngpus = 0;
    int rc = lhwloc.get_nobjs_in_cpuset(QV_HW_OBJ_GPU, machine.cdata(), ngpus);
    ctu_assert(
        rc == QV_SUCCESS,
        "local get_nobjs_in_cpuset(GPU) failed (rc=%s)", qv_strerr(rc)
    );

    if (ngpus == 0) {
        printf("# ✓ get_device_in_cpuset: no GPUs present, skipping match\n");
        return;
    }

    for (size_t i = 0; i < ngpus; ++i) {
        std::string rmi_id;
        rc = client->get_device_in_cpuset(
            QV_HW_OBJ_GPU, (int)i, machine, QV_DEVICE_ID_ORDINAL, rmi_id
        );
        ctu_assert(
            rc == QV_SUCCESS,
            "get_device_in_cpuset(GPU, %zu) failed (rc=%s)", i, qv_strerr(rc)
        );

        std::string local_id;
        rc = lhwloc.get_device_id_in_cpuset(
            QV_HW_OBJ_GPU, (int)i, machine.cdata(),
            QV_DEVICE_ID_ORDINAL, local_id
        );
        ctu_assert(
            rc == QV_SUCCESS,
            "local get_device_id_in_cpuset(GPU, %zu) failed (rc=%s)",
            i, qv_strerr(rc)
        );

        ctu_assert(
            rmi_id == local_id,
            "get_device_in_cpuset(GPU, %zu) mismatch: rmi=%s local=%s",
            i, rmi_id.c_str(), local_id.c_str()
        );

        printf("# ✓ get_device_in_cpuset(GPU, %zu) = %s\n", i, rmi_id.c_str());
    }
}

/**
 * Verifies get_intrinsic_hwpool for the intrinsic scopes. QV_SCOPE_USER and
 * QV_SCOPE_PROCESS must succeed and yield a non-empty cpuset; QV_SCOPE_SYSTEM
 * is documented as unsupported and must report that (not silently succeed).
 */
static void
check_get_intrinsic_hwpool(
    qvi_rmi_client *client,
    qvi_hwloc &lhwloc,
    pid_t who
) {
    // USER scope: no specific PIDs required.
    {
        qvi_hwpool hwpool;
        int rc = client->get_intrinsic_hwpool(
            std::vector<pid_t>{}, QV_SCOPE_USER, QV_SCOPE_FLAG_NONE, hwpool
        );
        ctu_assert(
            rc == QV_SUCCESS,
            "get_intrinsic_hwpool(USER) failed (rc=%s)", qv_strerr(rc)
        );
        ctu_assert(
            !hwloc_bitmap_iszero(hwpool.cpuset().cdata()),
            "get_intrinsic_hwpool(USER) yielded an empty cpuset"
        );
        // USER scope is the whole available machine cpuset.
        const qvi_hwloc_bitmap machine(lhwloc.topology_get_cpuset());
        ctu_assert(
            hwpool.cpuset() == machine,
            "get_intrinsic_hwpool(USER) cpuset=%s != machine=%s",
            qvi_hwloc::bitmap_string(hwpool.cpuset()).c_str(),
            qvi_hwloc::bitmap_string(machine).c_str()
        );
        printf("# [%d] ✓ get_intrinsic_hwpool(USER) = %s\n",
            who, qvi_hwloc::bitmap_string(hwpool.cpuset()).c_str());
    }
    // PROCESS scope: requires exactly one PID (the caller).
    {
        qvi_hwpool hwpool;
        int rc = client->get_intrinsic_hwpool(
            std::vector<pid_t>{who}, QV_SCOPE_PROCESS, QV_SCOPE_FLAG_NONE, hwpool
        );
        ctu_assert(
            rc == QV_SUCCESS,
            "get_intrinsic_hwpool(PROCESS) failed (rc=%s)", qv_strerr(rc)
        );
        // PROCESS scope must match the caller's current binding.
        qvi_hwloc_bitmap local_bitmap;
        rc = lhwloc.task_get_cpubind(who, local_bitmap);
        ctu_assert(
            rc == QV_SUCCESS,
            "local task_get_cpubind() failed (rc=%s)", qv_strerr(rc)
        );
        ctu_assert(
            hwpool.cpuset() == local_bitmap,
            "get_intrinsic_hwpool(PROCESS) cpuset=%s != binding=%s",
            qvi_hwloc::bitmap_string(hwpool.cpuset()).c_str(),
            qvi_hwloc::bitmap_string(local_bitmap).c_str()
        );
        printf("# [%d] ✓ get_intrinsic_hwpool(PROCESS) = %s\n",
            who, qvi_hwloc::bitmap_string(hwpool.cpuset()).c_str());
    }
    // SYSTEM scope: explicitly unsupported per the server implementation.
    {
        qvi_hwpool hwpool;
        int rc = client->get_intrinsic_hwpool(
            std::vector<pid_t>{}, QV_SCOPE_SYSTEM, QV_SCOPE_FLAG_NONE, hwpool
        );
        ctu_assert(
            rc == QV_ERR_NOT_SUPPORTED,
            "get_intrinsic_hwpool(SYSTEM) expected QV_ERR_NOT_SUPPORTED, got %s",
            qv_strerr(rc)
        );
        printf("# [%d] ✓ get_intrinsic_hwpool(SYSTEM) reported unsupported\n",
            who);
    }
}

static int
client(
    char *url,
    bool send_shutdown_msg
) {
    printf("# [%d] Starting Client (%s)\n", getpid(), url);

    char const *ers = nullptr;
    int portno = 0;
    const pid_t who = qvi_gettid();

    qvi_rmi_client *client = nullptr;
    int rc = qvi_new(&client);
    if (rc != QV_SUCCESS) {
        ers = "qvi_new(&client) failed";
        goto out;
    }

    rc = get_portno(url, &portno);
    if (rc != 0) {
        ers = "get_portno() failed";
        goto out;
    }

    rc = client->connect(QV_SCOPE_FLAG_NONE, url, portno);
    if (rc != QV_SUCCESS) {
        ers = "client->connect() failed";
        goto out;
    }

    // The client's hwloc instance is populated during connect() from the
    // server-exported topology; use it as the local ground truth.
    {
        qvi_hwloc &lhwloc = client->hwloc();

        check_get_cpubind(client, lhwloc, who);
        check_set_cpubind(client, who);
        check_get_obj_depth(client, lhwloc);
        check_get_nobjs_in_cpuset(client, lhwloc);
        check_get_cpuset_for_nobjs(client, lhwloc);
        check_get_device_in_cpuset(client, lhwloc);
        check_get_intrinsic_hwpool(client, lhwloc, who);
    }

    if (send_shutdown_msg) {
        rc = client->send_shutdown_message();
        if (rc != QV_SUCCESS) {
            ers = "client->send_shutdown_message() failed";
            goto out;
        }
        printf("# [%d] ✓ sent shutdown message\n", who);
    }
out:
    qvi_delete(&client);
    if (ers) {
        fprintf(stderr, "\n%s (rc=%d, %s)\n", ers, rc, qv_strerr(rc));
        return 1;
    }
    printf("# [%d] ✓ All client checks PASSED\n", who);
    return 0;
}

static void
usage(const char *appn)
{
    fprintf(stderr, "Usage: %s URL -s|-c|-cc\n", appn);
}

int
main(
    int argc,
    char **argv
) {
    int rc = 0;

    setbuf(stdout, nullptr);

    if (argc != 3) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (strcmp(argv[2], "-s") == 0) {
        rc = server(argv[1]);
    }
    else if (strcmp(argv[2], "-c") == 0) {
        rc = client(argv[1], false);
    }
    else if (strcmp(argv[2], "-cc") == 0) {
        rc = client(argv[1], true);
    }
    else {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    return (rc == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
