/* -*- Mode: C; c-basic-offset:4; indent-tabs-mode:nil -*- */

/**
 * @file test-process-hardware.c
 */

#include "quo-vadis.h"
#include "common-test-utils.h"

typedef struct {
    char *name;
    qv_scope_flags_t flags;
} setup_name_to_flags_t;

int
main(void)
{
    const setup_name_to_flags_t setup_tab[] = {
        {"QV_SCOPE_FLAG_NONE",   QV_SCOPE_FLAG_NONE},
        {"QV_SCOPE_FLAG_NO_SMT", QV_SCOPE_FLAG_NO_SMT}
    };

    const size_t n_setups = sizeof(setup_tab) / sizeof(setup_name_to_flags_t);
    for (size_t i = 0; i < n_setups; i++) {
        qv_scope_t *base_scope;
        ctu_check(
            qv_process_scope(QV_SCOPE_USER, setup_tab[i].flags, &base_scope)
        );

        ctu_emit_host_hw_info(
            base_scope, CTU_SCOPE_KIND_PROCESS, setup_tab[i].name
        );
        ctu_emit(base_scope, CTU_SCOPE_KIND_PROCESS, "\n");
        ctu_emit_device_info(
            base_scope, CTU_SCOPE_KIND_PROCESS,
            QV_HW_GPU, setup_tab[i].name
        );
        ctu_emit(base_scope, CTU_SCOPE_KIND_PROCESS, "\n");
        ctu_emit_device_info(
            base_scope, CTU_SCOPE_KIND_PROCESS,
            QV_HW_NIC, setup_tab[i].name
        );
        ctu_emit(base_scope, CTU_SCOPE_KIND_PROCESS, "\n");

        ctu_check(qv_free(base_scope));
    }
    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
