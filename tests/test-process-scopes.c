/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */

#include "quo-vadis.h"
#include "common-test-utils.h"

int
main(void)
{
    char const *ers = NULL;
    int rc = QV_SUCCESS;

    qv_scope_t *self_scope = NULL;
    rc = qv_process_scope_get(
        QV_SCOPE_PROCESS, QV_SCOPE_FLAG_NONE, &self_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_get(QV_SCOPE_PROCESS) failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    ctu_emit_scope_report(
        self_scope, CTU_SCOPE_KIND_PROCESS, "     self_scope"
    );

    rc = qv_scope_free(self_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    qv_scope_t *base_scope;
    rc = qv_process_scope_get(
        QV_SCOPE_USER, QV_SCOPE_FLAG_NONE, &base_scope
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_get(QV_SCOPE_USER) failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    ctu_emit_scope_report(
        base_scope, CTU_SCOPE_KIND_PROCESS, "     base_scope"
    );

    int sgsize;
    rc = qv_scope_group_size(base_scope, &sgsize);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_group_size() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    if (sgsize != 1) {
        ers = "Invalid number of tasks detected";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    ctu_emit_host_hw_info(
        base_scope, CTU_SCOPE_KIND_PROCESS, "     base_scope"
    );
    ctu_emit(base_scope, CTU_SCOPE_KIND_PROCESS, "\n");

    const int npieces = 2;
    // Provided color in range, so we will get the LHS of the split.
    // That is, with 2 pieces, the in-range coloring values are 0 and 1.
    qv_scope_t *sub_scope_left;
    rc = qv_scope_split(
        base_scope, npieces, 0, &sub_scope_left
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }
    // Provided color in range, so we will get the RHS of the split.
    // That is, with 2 pieces, the in-range coloring values are 0 and 1.
    qv_scope_t *sub_scope_right;
    rc = qv_scope_split(
        base_scope, npieces, 1, &sub_scope_right
    );
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_split() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    ctu_emit_host_hw_info(
        sub_scope_left, CTU_SCOPE_KIND_PROCESS, " sub_scope_left"
    );
    ctu_emit_scope_report(
        sub_scope_left, CTU_SCOPE_KIND_PROCESS, " sub_scope_left"
    );
    ctu_emit(sub_scope_left, CTU_SCOPE_KIND_PROCESS, "\n");
    ctu_emit_host_hw_info(
        sub_scope_right, CTU_SCOPE_KIND_PROCESS, "sub_scope_right"
    );
    ctu_emit_scope_report(
        sub_scope_right, CTU_SCOPE_KIND_PROCESS, "sub_scope_right"
    );

    rc = qv_scope_free(base_scope);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_scope_free(sub_scope_left);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    rc = qv_scope_free(sub_scope_right);
    if (rc != QV_SUCCESS) {
        ers = "qv_scope_free() failed";
        ctu_panic("%s (rc=%s)", ers, qv_strerr(rc));
    }

    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
