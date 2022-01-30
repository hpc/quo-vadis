/*
 * Copyright (c) 2020-2022 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-bbuff-rmi.cc
 */

/*
 * Picture Reference:
 * b = void *, size_t (two arguments)
 * c = hwloc_cpuset_t
 * h = qvi_line_hwpool_t *
 * i = int
 * s = char *
 * u = unsigned
 * z = empty (zero arguments)
 */

#include "qvi-common.h"
#include "qvi-bbuff-rmi.h"
#include "qvi-line.h"
#include "qvi-hwloc.h"

static cstr NULL_CPUSET = "";

/**
 *
 */
static inline int
pack_b(
    void *data,
    size_t dsize,
    qvi_bbuff_t *buff
) {
    // We store size then data so unpack has an easier time, but keep
    // the user interface order as data then size.
    int rc = qvi_bbuff_append(buff, &dsize, sizeof(dsize));
    if (rc != QV_SUCCESS) return rc;
    return qvi_bbuff_append(buff, data, dsize);
}

/**
 *
 */
static inline int
pack_c(
    hwloc_cpuset_t data,
    qvi_bbuff_t *buff
) {
    int rc = QV_SUCCESS;
    char *datas = nullptr;
    // Protect against null data.
    if (!data) {
        int np = asprintf(&datas, "%s", NULL_CPUSET);
        if (np == -1) return QV_ERR_OOR;
    }
    else {
        rc = qvi_hwloc_bitmap_asprintf(&datas, data);
        if (rc != QV_SUCCESS) goto out;
    }
    // We are sending the string representation of the cpuset.
    rc = qvi_bbuff_append(buff, datas, strlen(datas) + 1);
out:
    if (datas) free(datas);
    return rc;
}

/**
 *
 */
static inline int
pack_i(
    int data,
    qvi_bbuff_t *buff
) {
    return qvi_bbuff_append(buff, &data, sizeof(data));
}

/**
 *
 */
static inline int
pack_h(
    qvi_line_hwpool_t *data,
    qvi_bbuff_t *buff
) {
    // Pack cpuset.
    int rc = pack_c(data->cpuset, buff);
    if (rc != QV_SUCCESS) return rc;
    // Pack device IDs.
    const qv_hw_obj_type_t *devts = qvi_hwloc_supported_devices();
    for (int i = 0; devts[i] != QV_HW_OBJ_LAST; ++i) {
        const int nids = qvi_line_hwpool_ndevids(data, i);
        const size_t size = nids * sizeof(int);
        rc = pack_b(data->device_tab[i], size, buff);
        if (rc != QV_SUCCESS) break;
    }
    return rc;
}

/**
 *
 */
static inline int
pack_s(
    char *data,
    qvi_bbuff_t *buff
) {
    return qvi_bbuff_append(buff, data, strlen(data) + 1);
}

/**
 *
 */
static inline int
pack_u(
    unsigned data,
    qvi_bbuff_t *buff
) {
    return qvi_bbuff_append(buff, &data, sizeof(data));
}

int
qvi_bbuff_rmi_vsprintf(
    qvi_bbuff_t *buff,
    const char *picture,
    va_list args
) {
    int rc = QV_SUCCESS;

    const int npic = strlen(picture);
    for (int i = 0; i < npic; ++i) {
        if (picture[i] == 'b') {
            void *data = va_arg(args, void *);
            size_t dsize = va_arg(args, size_t);
            rc = pack_b(data, dsize, buff);
            if (rc != QV_SUCCESS) break;
            continue;
        }
        if (picture[i] == 'c') {
            hwloc_cpuset_t data = va_arg(args, hwloc_cpuset_t);
            rc = pack_c(data, buff);
            if (rc != QV_SUCCESS) break;
            continue;
        }
        if (picture[i] == 'h') {
            qvi_line_hwpool_t *data = va_arg(args, qvi_line_hwpool_t *);
            rc = pack_h(data, buff);
            if (rc != QV_SUCCESS) break;
            continue;
        }
        if (picture[i] == 'i') {
            int data = va_arg(args, int);
            rc = pack_i(data, buff);
            if (rc != QV_SUCCESS) break;
            continue;
        }
        if (picture[i] == 's') {
            char *data = va_arg(args, char *);
            rc = pack_s(data, buff);
            if (rc != QV_SUCCESS) break;
            continue;
        }
        if (picture[i] == 'u') {
            unsigned data = va_arg(args, unsigned);
            rc = pack_u(data, buff);
            if (rc != QV_SUCCESS) break;
            continue;
        }
        if (picture[i] == 'z') {
            continue;
        }
        else {
            rc = QV_ERR_INVLD_ARG;
            break;
        }
    }
    return rc;
}

int
qvi_bbuff_rmi_sprintf(
    qvi_bbuff_t *buff,
    const char *picture,
    ...
) {
    va_list vl;
    va_start(vl, picture);
    int rc = qvi_bbuff_rmi_vsprintf(buff, picture, vl);
    va_end(vl);
    return rc;
}

/**
 *
 */
static inline int
unpack_b(
    void **data,
    size_t *dsize,
    byte *buffpos,
    size_t *bytes_written
) {
    memmove(dsize, buffpos, sizeof(*dsize));
    buffpos += sizeof(*dsize);
    *bytes_written = sizeof(*dsize);

    *data = calloc(*dsize, sizeof(byte));
    if (!*data) {
        return QV_ERR_OOR;
    }
    memmove(*data, buffpos, *dsize);
    *bytes_written += *dsize;

    return QV_SUCCESS;
}

/**
 *
 */
static inline int
unpack_c(
    hwloc_cpuset_t *cpuset,
    byte *buffpos,
    size_t *bytes_written
) {
    int rc = qvi_hwloc_bitmap_calloc(cpuset);
    if (rc != QV_SUCCESS) return rc;

    char *cpusets = (char *)buffpos;
    // Protect against empty data.
    if (strcmp(NULL_CPUSET, cpusets) != 0) {
        rc = qvi_hwloc_bitmap_sscanf(*cpuset, cpusets);
        if (rc != QV_SUCCESS) goto out;
    }
    *bytes_written = strlen(cpusets) + 1;
out:
    if (rc != QV_SUCCESS) {
        hwloc_bitmap_free(*cpuset);
    }
    return rc;
}

/**
 *
 */
static inline int
unpack_i(
    int *i,
    byte *buffpos,
    size_t *bytes_written
) {
    memmove(i, buffpos, sizeof(*i));
    *bytes_written = sizeof(*i);
    return QV_SUCCESS;
}

/**
 *
 */
static inline int
unpack_h(
    qvi_line_hwpool_t **hwp,
    byte *buffpos,
    size_t *bytes_written
) {
    const qv_hw_obj_type_t *devts = qvi_hwloc_supported_devices();
    const int ndevts = qvi_hwloc_n_supported_devices();

    int rc = QV_SUCCESS;
    size_t bw = 0, total_bw = 0;

    qvi_line_hwpool_t *ihwp = nullptr;
    rc = qvi_line_hwpool_new(&ihwp);
    if (rc != QV_SUCCESS) goto out;
    // Unpack cpuset.
    rc = unpack_c(&ihwp->cpuset, buffpos, &bw);
    if (rc != QV_SUCCESS) goto out;
    total_bw += bw;
    buffpos += bw;
    // Unpack the device IDs.
    ihwp->device_tab = (int **)calloc(ndevts, sizeof(int *));
    if (!ihwp->device_tab) {
        rc = QV_ERR_OOR;
        goto out;
    }
    for (int i = 0; devts[i] != QV_HW_OBJ_LAST; ++i) {
        size_t dsize = 0;
        rc = unpack_b(
            (void **)&ihwp->device_tab[i],
            &dsize,
            buffpos,
            &bw
        );
        total_bw += bw;
        buffpos += bw;
    }
out:
    if (rc != QV_SUCCESS) {
        total_bw = 0;
        qvi_line_hwpool_free(&ihwp);
    }
    *bytes_written = total_bw;
    *hwp = ihwp;
    return rc;
}

/**
 *
 */
static inline int
unpack_s(
    char **s,
    byte *buffpos,
    size_t *bytes_written
) {
    const int nw = asprintf(s, "%s", buffpos);
    if (nw == -1) {
        *s = nullptr;
        *bytes_written = 0;
        return QV_ERR_OOR;
    }
    *bytes_written = nw + 1;
    return QV_SUCCESS;
}

/**
 *
 */
static inline int
unpack_u(
    unsigned *u,
    byte *buffpos,
    size_t *bytes_written
) {
    memmove(u, buffpos, sizeof(*u));
    *bytes_written = sizeof(*u);
    return QV_SUCCESS;
}

int
qvi_bbuff_rmi_vsscanf(
    void *data,
    const char *picture,
    va_list args
) {
    int rc = QV_SUCCESS;
    byte *pos = (byte *)data;
    size_t bytes_written = 0;

    const int npic = strlen(picture);
    for (int i = 0; i < npic; ++i) {
        if (picture[i] == 'b') {
            void **data = va_arg(args, void **);
            size_t *dsize = va_arg(args, size_t *);
            rc = unpack_b(data, dsize, pos, &bytes_written);
            if (rc != QV_SUCCESS) break;
            pos += bytes_written;
            continue;
        }
        if (picture[i] == 'c') {
            hwloc_cpuset_t *cpuset = va_arg(args, hwloc_cpuset_t *);
            rc = unpack_c(cpuset, pos, &bytes_written);
            if (rc != QV_SUCCESS) break;
            pos += bytes_written;
            continue;
        }
        if (picture[i] == 'h') {
            qvi_line_hwpool_t **hwreses =
                va_arg(args, qvi_line_hwpool_t **);
            rc = unpack_h(hwreses, pos, &bytes_written);
            if (rc != QV_SUCCESS) break;
            pos += bytes_written;
            continue;
        }
        if (picture[i] == 'i') {
            int *i = va_arg(args, int *);
            rc = unpack_i(i, pos, &bytes_written);
            if (rc != QV_SUCCESS) break;
            pos += bytes_written;
            continue;
        }
        if (picture[i] == 's') {
            char **s = va_arg(args, char **);
            rc = unpack_s(s, pos, &bytes_written);
            if (rc != QV_SUCCESS) break;
            pos += bytes_written;
            continue;
        }
        if (picture[i] == 'u') {
            unsigned *u = va_arg(args, unsigned *);
            rc = unpack_u(u, pos, &bytes_written);
            if (rc != QV_SUCCESS) break;
            pos += bytes_written;
            continue;
        }
        if (picture[i] == 'z') {
            continue;
        }
        else {
            rc = QV_ERR_INVLD_ARG;
            break;
        }
    }
    return rc;
}

int
qvi_bbuff_rmi_sscanf(
    void *data,
    const char *picture,
    ...
) {
    va_list vl;
    va_start(vl, picture);
    int rc = qvi_bbuff_rmi_vsscanf(data, picture, vl);
    va_end(vl);
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
