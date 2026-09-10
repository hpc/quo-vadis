/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */

#include "quo-vadis.h"
#include "common-test-utils.h"

typedef struct {
    char const *name;
    qv_scope_intrinsic_t iscope;
} intrinsic_name_to_scope_t;

// Maps QV intrinsic scope type names to their underlying values.
static const intrinsic_name_to_scope_t intrinsic_name_to_scope_tab[] = {
    {CTU_TOSTRING(QV_SCOPE_SYSTEM),  QV_SCOPE_SYSTEM},
    {CTU_TOSTRING(QV_SCOPE_USER),    QV_SCOPE_USER},
    {CTU_TOSTRING(QV_SCOPE_JOB),     QV_SCOPE_JOB},
    {CTU_TOSTRING(QV_SCOPE_PROCESS), QV_SCOPE_PROCESS}
};

static const size_t intrinsic_name_to_scope_tab_size =
    sizeof(intrinsic_name_to_scope_tab) / sizeof(intrinsic_name_to_scope_t);

int
main(void)
{
    char const *ers = NULL;
    int rc = QV_SUCCESS;

    // PU counts observed for each intrinsic scope type, indexed by the
    // qv_scope_intrinsic_t value (QV_SCOPE_SYSTEM=0, USER=1, JOB=2, PROCESS=3).
    int npus[QV_SCOPE_PROCESS + 1] = {0};

    for (size_t i = 0; i < intrinsic_name_to_scope_tab_size; ++i) {
        const char *name = intrinsic_name_to_scope_tab[i].name;
        const qv_scope_intrinsic_t iscope = intrinsic_name_to_scope_tab[i].iscope;

        qv_scope_t *scope = NULL;
        rc = qv_process_scope(iscope, QV_SCOPE_FLAG_NONE, &scope);
        if (rc != QV_SUCCESS) {
            ers = "qv_process_scope() failed";
            ctu_panic("%s for %s (rc=%s)", ers, name, qv_strerr(rc));
        }

        ctu_emit_scope_report(scope, CTU_SCOPE_KIND_PROCESS, name);
        ctu_emit_host_hw_info(scope, CTU_SCOPE_KIND_PROCESS, name);

        int sgsize = 0;
        rc = qv_group_size(scope, &sgsize);
        if (rc != QV_SUCCESS) {
            ers = "qv_group_size() failed";
            ctu_panic("%s for %s (rc=%s)", ers, name, qv_strerr(rc));
        }

        int sgrank = 0;
        rc = qv_group_rank(scope, &sgrank);
        if (rc != QV_SUCCESS) {
            ers = "qv_group_rank() failed";
            ctu_panic("%s for %s (rc=%s)", ers, name, qv_strerr(rc));
        }

        // Record the number of PUs in this scope for cross-scope comparison.
        int npu = 0;
        rc = qv_hw_obj_count(scope, QV_HW_OBJ_PU, &npu);
        if (rc != QV_SUCCESS) {
            ers = "qv_hw_obj_count(QV_HW_OBJ_PU) failed";
            ctu_panic("%s for %s (rc=%s)", ers, name, qv_strerr(rc));
        }
        npus[iscope] = npu;

        ctu_emit(
            scope, CTU_SCOPE_KIND_PROCESS,
            "%s group rank=%d size=%d npus=%d\n", name, sgrank, sgsize, npu
        );

        // For QV_SCOPE_PROCESS the group comprises only the calling process.
        if (iscope == QV_SCOPE_PROCESS && sgsize != 1) {
            ers = "Invalid number of tasks detected";
            ctu_panic("%s for %s (size=%d)", ers, name, sgsize);
        }

        // Split the base scope in half. Since only a single process is
        // involved, it can join each piece in turn by requesting the
        // corresponding color (0 for the left half, 1 for the right half).
        const int npieces = 2;
        qv_scope_t *left = NULL;
        rc = qv_split(scope, npieces, 0, &left);
        if (rc != QV_SUCCESS) {
            ers = "qv_split() failed";
            ctu_panic("%s for %s (rc=%s)", ers, name, qv_strerr(rc));
        }
        qv_scope_t *right = NULL;
        rc = qv_split(scope, npieces, 1, &right);
        if (rc != QV_SUCCESS) {
            ers = "qv_split() failed";
            ctu_panic("%s for %s (rc=%s)", ers, name, qv_strerr(rc));
        }

        int npu_left = 0;
        rc = qv_hw_obj_count(left, QV_HW_OBJ_PU, &npu_left);
        if (rc != QV_SUCCESS) {
            ers = "qv_hw_obj_count(QV_HW_OBJ_PU) failed";
            ctu_panic("%s for %s left half (rc=%s)", ers, name, qv_strerr(rc));
        }
        int npu_right = 0;
        rc = qv_hw_obj_count(right, QV_HW_OBJ_PU, &npu_right);
        if (rc != QV_SUCCESS) {
            ers = "qv_hw_obj_count(QV_HW_OBJ_PU) failed";
            ctu_panic("%s for %s right half (rc=%s)", ers, name, qv_strerr(rc));
        }

        // A split in half must conserve PUs and, for these topologies, divide
        // them evenly. These invariants hold regardless of the topology.
        if (npu % 2 != 0) {
            ctu_panic(
                "%s base PU count (%d) is not evenly divisible by 2",
                name, npu
            );
        }
        if (npu_left + npu_right != npu) {
            ctu_panic(
                "%s split(2): left (%d) + right (%d) != base (%d)",
                name, npu_left, npu_right, npu
            );
        }
        if (npu_left != npu / 2 || npu_right != npu / 2) {
            ctu_panic(
                "%s split(2) is unbalanced: left=%d right=%d (expected %d each)",
                name, npu_left, npu_right, npu / 2
            );
        }

        // Bind to each half and report the resulting binding. For the
        // QV_SCOPE_SYSTEM case the whole system is split into an allowed half
        // (the first package, PUs 0-15) and a disallowed half (the second
        // package, PUs 16-31). Binding to the disallowed half must still work
        // and must report those disallowed PUs, because the parent scope
        // explicitly requested the whole system. For the other intrinsic scopes
        // both halves only ever cover allowed resources.
        char *bind_left = NULL;
        rc = qv_bind_push(left);
        if (rc != QV_SUCCESS) {
            ers = "qv_bind_push() failed";
            ctu_panic("%s for %s left half (rc=%s)", ers, name, qv_strerr(rc));
        }
        rc = qv_bind_string(left, QV_BIND_STRING_PHYSICAL, &bind_left);
        if (rc != QV_SUCCESS) {
            ers = "qv_bind_string() failed";
            ctu_panic("%s for %s left half (rc=%s)", ers, name, qv_strerr(rc));
        }
        if (bind_left == NULL || bind_left[0] == '\0') {
            ctu_panic("%s left half produced an empty binding string", name);
        }
        rc = qv_bind_pop(left);
        if (rc != QV_SUCCESS) {
            ers = "qv_bind_pop() failed";
            ctu_panic("%s for %s left half (rc=%s)", ers, name, qv_strerr(rc));
        }

        char *bind_right = NULL;
        rc = qv_bind_push(right);
        if (rc != QV_SUCCESS) {
            ers = "qv_bind_push() failed";
            ctu_panic("%s for %s right half (rc=%s)", ers, name, qv_strerr(rc));
        }
        rc = qv_bind_string(right, QV_BIND_STRING_PHYSICAL, &bind_right);
        if (rc != QV_SUCCESS) {
            ers = "qv_bind_string() failed";
            ctu_panic("%s for %s right half (rc=%s)", ers, name, qv_strerr(rc));
        }
        if (bind_right == NULL || bind_right[0] == '\0') {
            ctu_panic("%s right half produced an empty binding string", name);
        }
        rc = qv_bind_pop(right);
        if (rc != QV_SUCCESS) {
            ers = "qv_bind_pop() failed";
            ctu_panic("%s for %s right half (rc=%s)", ers, name, qv_strerr(rc));
        }

        ctu_emit(
            scope, CTU_SCOPE_KIND_PROCESS,
            "%s split(2): left=%d right=%d base=%d "
            "left-bind=%s right-bind=%s\n",
            name, npu_left, npu_right, npu, bind_left, bind_right
        );

        // The expected number of PUs per half is known for the restricted
        // synthetic topology: QV_SCOPE_SYSTEM sees the whole system (32 PUs,
        // including the disallowed package), so each half has 16; the
        // cgroup-obeying scopes see 16 PUs, so each half has 8.
        if (getenv("QVI_TEST_EXPECT_RESTRICTED") != NULL) {
            const int expected_half = (iscope == QV_SCOPE_SYSTEM) ? 16 : 8;
            if (npu_left != expected_half) {
                ctu_panic(
                    "restricted %s split(2): each half must have %d PUs, "
                    "got %d",
                    name, expected_half, npu_left
                );
            }
            // For QV_SCOPE_SYSTEM the right half is the disallowed package
            // (physical PUs 16-31). Binding to it must succeed and must report
            // those disallowed PUs, proving the disallowed resources remain
            // usable when inherited from the whole-system scope.
            if (iscope == QV_SCOPE_SYSTEM) {
                if (strcmp(bind_right, "P16-31") != 0) {
                    ctu_panic(
                        "restricted QV_SCOPE_SYSTEM right half must bind to "
                        "the disallowed package (P16-31), got %s",
                        bind_right
                    );
                }
            }
        }

        free(bind_left);
        free(bind_right);

        rc = qv_free(left);
        if (rc != QV_SUCCESS) {
            ers = "qv_free() failed";
            ctu_panic("%s for %s left half (rc=%s)", ers, name, qv_strerr(rc));
        }
        rc = qv_free(right);
        if (rc != QV_SUCCESS) {
            ers = "qv_free() failed";
            ctu_panic("%s for %s right half (rc=%s)", ers, name, qv_strerr(rc));
        }

        ctu_emit(scope, CTU_SCOPE_KIND_PROCESS, "\n");

        rc = qv_free(scope);
        if (rc != QV_SUCCESS) {
            ers = "qv_free() failed";
            ctu_panic("%s for %s (rc=%s)", ers, name, qv_strerr(rc));
        }
    }

    // The intrinsic scopes are nested by construction: the whole-system scope
    // (QV_SCOPE_SYSTEM) is a superset of the allowed-resource scope
    // (QV_SCOPE_USER), which in turn is a superset of the per-process scope
    // (QV_SCOPE_PROCESS). These invariants must hold on any topology.
    const int npu_system  = npus[QV_SCOPE_SYSTEM];
    const int npu_user    = npus[QV_SCOPE_USER];
    const int npu_process = npus[QV_SCOPE_PROCESS];

    if (npu_user < 1) {
        ctu_panic(
            "QV_SCOPE_USER has no PUs (npus=%d)", npu_user
        );
    }
    if (npu_system < npu_user) {
        ctu_panic(
            "QV_SCOPE_SYSTEM PUs (%d) < QV_SCOPE_USER PUs (%d)",
            npu_system, npu_user
        );
    }
    if (npu_user < npu_process) {
        ctu_panic(
            "QV_SCOPE_USER PUs (%d) < QV_SCOPE_PROCESS PUs (%d)",
            npu_user, npu_process
        );
    }

    // When the test is run against a topology that restricts the allowed
    // resources (simulating a cgroup), QV_SCOPE_SYSTEM must see strictly more
    // PUs than QV_SCOPE_USER, since SYSTEM includes disallowed resources.
    if (getenv("QVI_TEST_EXPECT_RESTRICTED") != NULL) {
        if (npu_system <= npu_user) {
            ctu_panic(
                "expected restricted topology: QV_SCOPE_SYSTEM PUs (%d) "
                "must exceed QV_SCOPE_USER PUs (%d)",
                npu_system, npu_user
            );
        }
    }

    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
