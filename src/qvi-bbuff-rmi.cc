/*
 * Copyright (c) 2020-2021 Triad National Security, LLC
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
 * i = int
 * s = char *
 * u = unsigned
 * z = empty (zero arguments)
 */

#include "qvi-common.h"
#include "qvi-bbuff-rmi.h"
#include "qvi-hwloc.h"

static cstr NULL_CPUSET = "";

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
            // We store size then data so unpack has an easier time, but keep
            // the user interface order as data then size.
            rc = qvi_bbuff_append(buff, &dsize, sizeof(dsize));
            if (rc != QV_SUCCESS) break;
            rc = qvi_bbuff_append(buff, data, dsize);
            if (rc != QV_SUCCESS) break;
            continue;
        }
        if (picture[i] == 'c') {
            hwloc_cpuset_t data = va_arg(args, hwloc_cpuset_t);
            char *datas = nullptr;
            // Protect against null data.
            if (!data) {
                int np = asprintf(&datas, "%s", NULL_CPUSET);
                if (np == -1) {
                    rc = QV_ERR_OOR;
                    break;
                }
            }
            else {
                rc = qvi_hwloc_bitmap_asprintf(&datas, data);
                if (rc != QV_SUCCESS) break;
            }
            // We are sending the string representation of the cpuset.
            rc = qvi_bbuff_append(buff, datas, strlen(datas) + 1);
            free(datas);
            if (rc != QV_SUCCESS) break;
            continue;
        }
        if (picture[i] == 'i') {
            int data = va_arg(args, int);
            rc = qvi_bbuff_append(buff, &data, sizeof(data));
            if (rc != QV_SUCCESS) break;
            continue;
        }
        if (picture[i] == 's') {
            char *data = va_arg(args, char *);
            rc = qvi_bbuff_append(buff, data, strlen(data) + 1);
            if (rc != QV_SUCCESS) break;
            continue;
        }
        if (picture[i] == 'u') {
            unsigned data = va_arg(args, unsigned);
            rc = qvi_bbuff_append(buff, &data, sizeof(data));
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

int
qvi_data_rmi_vsscanf(
    void *data,
    const char *picture,
    va_list args
) {
    int rc = QV_SUCCESS;
    byte *pos = (byte *)data;

    const int npic = strlen(picture);
    for (int i = 0; i < npic; ++i) {
        if (picture[i] == 'b') {
            void **data = va_arg(args, void **);
            size_t *dsize = va_arg(args, size_t *);
            memmove(dsize, pos, sizeof(*dsize));
            pos += sizeof(*dsize);
            *data = calloc(*dsize, sizeof(byte));
            if (!*data) {
                rc = QV_ERR_OOR;
                break;
            }
            memmove(*data, pos, *dsize);
            pos += *dsize;
            continue;
        }
        if (picture[i] == 'c') {
            hwloc_cpuset_t *cpuset = va_arg(args, hwloc_cpuset_t *);
            rc = qvi_hwloc_bitmap_alloc(cpuset);
            if (rc != QV_SUCCESS) break;
            char *cpusets = (char *)pos;
            // Protect against empty data.
            if (strcmp(NULL_CPUSET, cpusets) == 0) {
                hwloc_bitmap_zero(*cpuset);
            }
            else {
                rc = qvi_hwloc_bitmap_sscanf(*cpuset, cpusets);
                if (rc != QV_SUCCESS) break;
            }
            pos += strlen(cpusets) + 1;
            continue;
        }
        if (picture[i] == 'i') {
            memmove(va_arg(args, int *), pos, sizeof(int));
            pos += sizeof(int);
            continue;
        }
        if (picture[i] == 's') {
            const int nw = asprintf(va_arg(args, char **), "%s", pos);
            if (nw == -1) {
                rc = QV_ERR_OOR;
                break;
            }
            pos += nw + 1;
            continue;
        }
        if (picture[i] == 'u') {
            memmove(va_arg(args, unsigned *), pos, sizeof(unsigned));
            pos += sizeof(unsigned);
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
qvi_data_rmi_sscanf(
    void *data,
    const char *picture,
    ...
) {
    va_list vl;
    va_start(vl, picture);
    int rc = qvi_data_rmi_vsscanf(data, picture, vl);
    va_end(vl);
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
