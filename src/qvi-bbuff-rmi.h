/*
 * Copyright (c) 2021-2022 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-bbuff-rmi.h
 */

/*
 * Picture Reference:
 * b = qvi_bbuff_rmi_bytes_in_t, qvi_bbuff_rmi_bytes_out_t
 * c = hwloc_cpuset_t
 * h = qvi_hwpool_t * (conversion to qvi_line_hwpool_t * done internally)
 * h = qvi_line_hwpool_t *
 * i = int
 * s = char *
 * t = qv_task_type_t, qvi_task_id_t
 * z = qvi_bbuff_rmi_zero_msg_t
 */

#ifndef QVI_BBUFF_RMI_H
#define QVI_BBUFF_RMI_H

#include "qvi-common.h"
#include "qvi-hwloc.h"
#include "qvi-hwpool.h"

#include <string>

// 'Null' cpuset representation as a string.
#define QV_BUFF_RMI_NULL_CPUSET ""

// Byte containers
using qvi_bbuff_rmi_bytes_in_t = std::pair<void *, size_t>;
using qvi_bbuff_rmi_bytes_out_t = std::pair<void **, size_t *>;

// Defines a zero message type.
typedef enum qvi_bbuff_rmi_zero_msg_e {
    QVI_BBUFF_RMI_ZERO_MSG = 0
} qvi_bbuff_rmi_zero_msg_t;

////////////////////////////////////////////////////////////////////////////////
// Pack
////////////////////////////////////////////////////////////////////////////////
template<typename T>
inline void
qvi_bbuff_rmi_pack_type_picture(
    std::string &picture,
    T
);

template<>
inline void
qvi_bbuff_rmi_pack_type_picture(
    std::string &picture,
    qvi_bbuff_rmi_bytes_in_t
) {
    picture += "b";
}

template<>
inline void
qvi_bbuff_rmi_pack_type_picture(
    std::string &picture,
    hwloc_const_cpuset_t
) {
    picture += "c";
}

template<>
inline void
qvi_bbuff_rmi_pack_type_picture(
    std::string &picture,
    hwloc_cpuset_t
) {
    picture += "c";
}

template<>
inline void
qvi_bbuff_rmi_pack_type_picture(
    std::string &picture,
    hwloc_cpuset_t *
) {
    picture += "c";
}

template<>
inline void
qvi_bbuff_rmi_pack_type_picture(
    std::string &picture,
    qvi_line_hwpool_t *
) {
    picture += "h";
}

template<>
inline void
qvi_bbuff_rmi_pack_type_picture(
    std::string &picture,
    const qvi_hwpool_t *
) {
    picture += "h";
}

template<>
inline void
qvi_bbuff_rmi_pack_type_picture(
    std::string &picture,
    qvi_hwpool_t *
) {
    picture += "h";
}

template<>
inline void
qvi_bbuff_rmi_pack_type_picture(
    std::string &picture,
    qvi_hwpool_t **
) {
    picture += "h";
}

template<>
inline void
qvi_bbuff_rmi_pack_type_picture(
    std::string &picture,
    int
) {
    picture += "i";
}

template<>
inline void
qvi_bbuff_rmi_pack_type_picture(
    std::string &picture,
    int *
) {
    picture += "i";
}

template<>
inline void
qvi_bbuff_rmi_pack_type_picture(
    std::string &picture,
    qv_hw_obj_type_t
) {
    picture += "i";
}

template<>
inline void
qvi_bbuff_rmi_pack_type_picture(
    std::string &picture,
    qv_device_id_type_t
) {
    picture += "i";
}

template<>
inline void
qvi_bbuff_rmi_pack_type_picture(
    std::string &picture,
    qv_scope_intrinsic_t
) {
    picture += "i";
}

#if QVI_SIZEOF_INT != QVI_SIZEOF_PID_T
template<>
inline void
qvi_bbuff_rmi_pack_type_picture(
    std::string &picture,
    pid_t
) {
    picture += "i";
}
#endif

template<>
inline void
qvi_bbuff_rmi_pack_type_picture(
    std::string &picture,
    cstr_t
) {
    picture += "s";
}

template<>
inline void
qvi_bbuff_rmi_pack_type_picture(
    std::string &picture,
    char *
) {
    picture += "s";
}

template<>
inline void
qvi_bbuff_rmi_pack_type_picture(
    std::string &picture,
    char **
) {
    picture += "s";
}

template<>
inline void
qvi_bbuff_rmi_pack_type_picture(
    std::string &picture,
    qvi_bbuff_rmi_zero_msg_t
) {
    picture += "z";
}

template<>
inline void
qvi_bbuff_rmi_pack_type_picture(
    std::string &picture,
    qvi_task_id_t
) {
    picture += "t";
}

template<>
inline void
qvi_bbuff_rmi_pack_type_picture(
    std::string &picture,
    qvi_task_id_t *
) {
    picture += "t";
}

template<>
inline void
qvi_bbuff_rmi_pack_type_picture(
    std::string &picture,
    qv_task_type_t
) {
    picture += "t";
}

template<>
inline void
qvi_bbuff_rmi_pack_type_picture(
    std::string &picture,
    qv_task_type_t *
) {
    picture += "t";
}

inline void
qvi_bbuff_rmi_get_picture(
    std::string &
) {
    // Base case
}

template<typename T, typename... Types>
inline void
qvi_bbuff_rmi_get_picture
(
    std::string &picture,
    T arg,
    Types... args
) {
    qvi_bbuff_rmi_pack_type_picture(picture, arg);
    qvi_bbuff_rmi_get_picture(picture, args...);
}

template<typename T>
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff_t *,
    T
);

/**
 * Packs int.
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff_t *buff,
    int data
) {
    return qvi_bbuff_append(buff, &data, sizeof(data));
}

/**
 * Packs qv_hw_obj_type_t as an int.
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff_t *buff,
    qv_hw_obj_type_t data
) {
    const int dai = (int)data;
    return qvi_bbuff_append(buff, &dai, sizeof(dai));
}

/**
 * Packs qv_device_id_type_t as an int.
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff_t *buff,
    qv_device_id_type_t data
) {
    const int dai = (int)data;
    return qvi_bbuff_append(buff, &dai, sizeof(dai));
}

/**
 * Packs qv_scope_intrinsic_t as an int.
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff_t *buff,
    qv_scope_intrinsic_t data
) {
    const int dai = (int)data;
    return qvi_bbuff_append(buff, &dai, sizeof(dai));
}

#if QVI_SIZEOF_INT != QVI_SIZEOF_PID_T
/**
 * Packs pid_t as an int.
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff_t *buff,
    pid_t data
) {
    const int dai = (int)data;
    return qvi_bbuff_append(buff, &dai, sizeof(dai));
}
#endif

/**
 * Packs qv_task_type_t as an int.
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff_t *buff,
    qv_task_type_t data
) {
    const int dai = (int)data;
    return qvi_bbuff_append(buff, &dai, sizeof(dai));
}

/**
 * Packs qvi_task_id_t as an int.
 */
/*
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff_t *buff,
    qvi_task_id_t data
) {
    const int dai = (int)data;
    return qvi_bbuff_append(buff, &dai, sizeof(dai));
}
*/

inline int
qvi_bbuff_rmi_pack_item_impl(
    qvi_bbuff_t *buff,
    cstr_t data
) {
    return qvi_bbuff_append(buff, data, strlen(data) + 1);
}

/**
 * Packs const char *
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff_t *buff,
    cstr_t data
) {
    return qvi_bbuff_rmi_pack_item_impl(buff, data);
}

/**
 * Packs char *
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff_t *buff,
    char *data
) {
    return qvi_bbuff_rmi_pack_item_impl(buff, data);
}

/**
 * Packs qvi_bbuff_rmi_zero_msg_t
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff_t *,
    qvi_bbuff_rmi_zero_msg_t
) {
    return QV_SUCCESS;
}

/**
 * Packs qvi_bbuff_rmi_bytes_in_t.
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff_t *buff,
    qvi_bbuff_rmi_bytes_in_t data
) {
    // We store size then data so unpack has an easier time, but keep
    // the user interface order as data then size.
    size_t dsize = data.second;
    int rc = qvi_bbuff_append(buff, &dsize, sizeof(dsize));
    if (rc != QV_SUCCESS) return rc;
    return qvi_bbuff_append(buff, data.first, dsize);
}

/**
 * Packs hwloc_const_cpuset_t.
 */
inline int
qvi_bbuff_rmi_pack_item_impl(
    qvi_bbuff_t *buff,
    hwloc_const_cpuset_t data
) {
    int rc = QV_SUCCESS;
    char *datas = nullptr;
    // Protect against null data.
    if (!data) {
        int np = asprintf(&datas, "%s", QV_BUFF_RMI_NULL_CPUSET);
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
 * Packs hwloc_cpuset_t.
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff_t *buff,
    hwloc_cpuset_t data
) {
    return qvi_bbuff_rmi_pack_item_impl(buff, data);
}

/**
 * Packs hwloc_const_cpuset_t
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff_t *buff,
    hwloc_const_cpuset_t data
) {
    return qvi_bbuff_rmi_pack_item_impl(buff, data);
}

/**
 * Packs qvi_line_devinfo_t *
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff_t *buff,
    qvi_line_devinfo_t *data
) {
    // Pack cpuset.
    int rc = qvi_bbuff_rmi_pack_item(buff, data->affinity);
    if (rc != QV_SUCCESS) return rc;
    // Pack device type.
    rc = qvi_bbuff_rmi_pack_item(buff, data->type);
    if (rc != QV_SUCCESS) return rc;
    // Pack device ID.
    rc = qvi_bbuff_rmi_pack_item(buff, data->id);
    if (rc != QV_SUCCESS) return rc;
    // Pack device PCI bus ID.
    rc = qvi_bbuff_rmi_pack_item(buff, data->pci_bus_id);
    if (rc != QV_SUCCESS) return rc;
    // Pack device UUID.
    rc = qvi_bbuff_rmi_pack_item(buff, data->uuid);
    if (rc != QV_SUCCESS) return rc;
    return rc;
}

/**
 * Packs qvi_line_hwpool_t *
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff_t *buff,
    qvi_line_hwpool_t *data
) {
    // Pack cpuset.
    int rc = qvi_bbuff_rmi_pack_item(buff, data->cpuset);
    if (rc != QV_SUCCESS) return rc;
    // Pack ndevinfos
    rc = qvi_bbuff_rmi_pack_item(buff, data->ndevinfos);
    if (rc != QV_SUCCESS) return rc;
    // Pack device infos.
    for (int i = 0; i < data->ndevinfos; ++i) {
        qvi_bbuff_rmi_pack_item(buff, &data->devinfos[i]);
    }
    return rc;
}

/**
 * Packs const qvi_hwpool_t *
 */
inline int
qvi_bbuff_rmi_pack_item_impl(
    qvi_bbuff_t *buff,
    const qvi_hwpool_t *data
) {
    // Convert input data to line protocol.
    qvi_line_hwpool_t *line = nullptr;
    int rc = qvi_hwpool_new_line_from_hwpool(data, &line);
    if (rc != QV_SUCCESS) return rc;
    // Pack the data
    rc = qvi_bbuff_rmi_pack_item(buff, line);
    qvi_line_hwpool_free(&line);
    return rc;
}

/**
 * Packs qvi_task_id_t 
 */
inline int
qvi_bbuff_rmi_pack_item_impl(
    qvi_bbuff_t *buff,
    qvi_task_id_t  data
) {
    // Pack task type.
    int rc = qvi_bbuff_rmi_pack_item(buff, data.type);
    if (rc != QV_SUCCESS) return rc;
    // Pack pid
    rc = qvi_bbuff_rmi_pack_item(buff, data.who);
    if (rc != QV_SUCCESS) return rc;
    return rc;
}

/**
 * Packs const qvi_hwpool_t *
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff_t *buff,
    const qvi_hwpool_t *data
) {
    return qvi_bbuff_rmi_pack_item_impl(buff, data);
}

/**
 * Packs qvi_hwpool_t *
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff_t *buff,
    qvi_hwpool_t *data
) {
    return qvi_bbuff_rmi_pack_item_impl(buff, data);
}

/**
 * Packs qvi_task_id_t 
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff_t *buff,
    qvi_task_id_t data
) {
    return qvi_bbuff_rmi_pack_item_impl(buff, data);
}

inline int
qvi_bbuff_rmi_pack(
    qvi_bbuff_t *
) {
    // Base case
    return QV_SUCCESS;
}

template<typename T, typename... Types>
inline int
qvi_bbuff_rmi_pack(
    qvi_bbuff_t *buff,
    T arg,
    Types... args
) {
    int rc = qvi_bbuff_rmi_pack_item(buff, arg);
    if (rc != QV_SUCCESS) return rc;
    return qvi_bbuff_rmi_pack(buff, args...);
}


////////////////////////////////////////////////////////////////////////////////
// Unpack
////////////////////////////////////////////////////////////////////////////////
template<typename T>
inline int
qvi_bbuff_rmi_unpack_item(
    T,
    byte_t *buffpos,
    size_t *bytes_written
);

inline int
qvi_bbuff_rmi_unpack_item(
    int *i,
    byte_t *buffpos,
    size_t *bytes_written
) {
    memmove(i, buffpos, sizeof(*i));
    *bytes_written = sizeof(*i);
    return QV_SUCCESS;
}

inline int
qvi_bbuff_rmi_unpack_item(
    qv_task_type_t *i,
    byte_t *buffpos,
    size_t *bytes_written
) {
    memmove(i, buffpos, sizeof(*i));
    *bytes_written = sizeof(*i);
    return QV_SUCCESS;
}

#if QVI_SIZEOF_INT != QVI_SIZEOF_PID_T
inline int
qvi_bbuff_rmi_unpack_item(
    pid_t *i,
    byte_t *buffpos,
    size_t *bytes_written
) {
    // Remember we are sending this as an int.
    int pai = 0;
    memmove(&pai, buffpos, sizeof(pai));
    *bytes_written = sizeof(pai);
    *i = (pid_t)pai;
    return QV_SUCCESS;
}
#endif

inline int
qvi_bbuff_rmi_unpack_item(
    char **s,
    byte_t *buffpos,
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

inline int
qvi_bbuff_rmi_unpack_item(
    qv_scope_intrinsic_t *o,
    byte_t *buffpos,
    size_t *bytes_written
) {
    // Remember we are sending this as an int.
    int oai = 0;
    memmove(&oai, buffpos, sizeof(oai));
    *bytes_written = sizeof(oai);
    *o = (qv_scope_intrinsic_t)oai;
    return QV_SUCCESS;
}

inline int
qvi_bbuff_rmi_unpack_item(
    qv_device_id_type_t *o,
    byte_t *buffpos,
    size_t *bytes_written
) {
    // Remember we are sending this as an int.
    int oai = 0;
    memmove(&oai, buffpos, sizeof(oai));
    *bytes_written = sizeof(oai);
    *o = (qv_device_id_type_t)oai;
    return QV_SUCCESS;
}

inline int
qvi_bbuff_rmi_unpack_item(
    qv_hw_obj_type_t *o,
    byte_t *buffpos,
    size_t *bytes_written
) {
    // Remember we are sending this as an int.
    int oai = 0;
    memmove(&oai, buffpos, sizeof(oai));
    *bytes_written = sizeof(oai);
    *o = (qv_hw_obj_type_t)oai;
    return QV_SUCCESS;
}

inline int
qvi_bbuff_rmi_unpack_item(
    qvi_bbuff_rmi_bytes_out_t data,
    byte_t *buffpos,
    size_t *bytes_written
) {
    size_t *dsize = data.second;
    memmove(dsize, buffpos, sizeof(*dsize));
    buffpos += sizeof(*dsize);
    *bytes_written = sizeof(*dsize);

    void **dbuff = data.first;
    *dbuff = calloc(*dsize, sizeof(byte_t));
    if (!*dbuff) {
        return QV_ERR_OOR;
    }
    memmove(*dbuff, buffpos, *dsize);
    *bytes_written += *dsize;

    return QV_SUCCESS;
}

inline int
qvi_bbuff_rmi_unpack_item(
    hwloc_cpuset_t *cpuset,
    byte_t *buffpos,
    size_t *bytes_written
) {
    int rc = qvi_hwloc_bitmap_calloc(cpuset);
    if (rc != QV_SUCCESS) return rc;

    char *cpusets = (char *)buffpos;
    // Protect against empty data.
    if (strcmp(QV_BUFF_RMI_NULL_CPUSET, cpusets) != 0) {
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

inline int
qvi_bbuff_rmi_unpack_item(
    qvi_bbuff_rmi_zero_msg_t,
    byte_t *,
    size_t *bytes_written
) {
    *bytes_written = 0;
    return QV_SUCCESS;
}

inline int
qvi_bbuff_rmi_unpack_item(
    qvi_line_devinfo_t *di,
    byte_t *buffpos,
    size_t *bytes_written
) {
    size_t bw = 0, total_bw = 0;

    int rc = qvi_bbuff_rmi_unpack_item(
        &di->affinity, buffpos, &bw
    );
    if (rc != QV_SUCCESS) goto out;
    total_bw += bw;
    buffpos += bw;

    rc = qvi_bbuff_rmi_unpack_item(
        &di->type, buffpos, &bw
    );
    if (rc != QV_SUCCESS) goto out;
    total_bw += bw;
    buffpos += bw;

    rc = qvi_bbuff_rmi_unpack_item(
        &di->id, buffpos, &bw
    );
    if (rc != QV_SUCCESS) goto out;
    total_bw += bw;
    buffpos += bw;

    rc = qvi_bbuff_rmi_unpack_item(
        &di->pci_bus_id, buffpos, &bw
    );
    if (rc != QV_SUCCESS) goto out;
    total_bw += bw;
    buffpos += bw;

    rc = qvi_bbuff_rmi_unpack_item(
        &di->uuid, buffpos, &bw
    );
    if (rc != QV_SUCCESS) goto out;
    total_bw += bw;
    buffpos += bw;
out:
    if (rc != QV_SUCCESS) {
        total_bw = 0;
    }
    *bytes_written = total_bw;
    return rc;
}

inline int
qvi_bbuff_rmi_unpack_item(
    qvi_line_hwpool_t **hwp,
    byte_t *buffpos,
    size_t *bytes_written
) {
    int rc = QV_SUCCESS;
    size_t bw = 0, total_bw = 0;

    qvi_line_hwpool_t *ihwp = nullptr;
    rc = qvi_line_hwpool_new(&ihwp);
    if (rc != QV_SUCCESS) goto out;
    // Unpack cpuset.
    rc = qvi_bbuff_rmi_unpack_item(
        &ihwp->cpuset, buffpos, &bw
    );
    if (rc != QV_SUCCESS) goto out;
    total_bw += bw;
    buffpos += bw;
    // Unpack number of devinfos.
    rc = qvi_bbuff_rmi_unpack_item(
        &ihwp->ndevinfos, buffpos, &bw
    );
    if (rc != QV_SUCCESS) goto out;
    total_bw += bw;
    buffpos += bw;
    // Unpack devinfos.
    ihwp->devinfos = qvi_new qvi_line_devinfo_t[ihwp->ndevinfos]();
    if (!ihwp->devinfos) {
        rc = QV_ERR_OOR;
        goto out;
    }
    for (int i = 0; i < ihwp->ndevinfos; ++i) {
        rc = qvi_bbuff_rmi_unpack_item(
            &ihwp->devinfos[i], buffpos, &bw
        );
        if (rc != QV_SUCCESS) break;
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

inline int
qvi_bbuff_rmi_unpack_item(
    qvi_hwpool_t **hwp,
    byte_t *buffpos,
    size_t *bytes_written
) {
    qvi_line_hwpool_t *line = nullptr;
    int rc = qvi_bbuff_rmi_unpack_item(
        &line,
        buffpos,
        bytes_written
    );
    if (rc != QV_SUCCESS) goto out;

    rc = qvi_hwpool_new_from_line(line, hwp);
out:
    qvi_line_hwpool_free(&line);
    if (rc != QV_SUCCESS) {
        *bytes_written = 0;
        qvi_hwpool_free(hwp);
    }
    return rc;
}

inline int
qvi_bbuff_rmi_unpack_item(
    qvi_task_id_t *taski,
    byte_t *buffpos,
    size_t *bytes_written
) {
    size_t bw = 0, total_bw = 0;

    int rc = qvi_bbuff_rmi_unpack_item(
        &taski->type, buffpos, &bw
    );
    if (rc != QV_SUCCESS) goto out;
    total_bw += bw;
    buffpos += bw;

    rc = qvi_bbuff_rmi_unpack_item(
        &taski->who, buffpos, &bw
    );
    if (rc != QV_SUCCESS) goto out;
    total_bw += bw;
    buffpos += bw;
out:
    if (rc != QV_SUCCESS) {
        total_bw = 0;
    }
    *bytes_written = total_bw;
    return rc;
}

/**
 * Base case for qvi_bbuff_rmi_unpack.
 */
inline int
qvi_bbuff_rmi_unpack(
    void *
) {
    return QV_SUCCESS;
}

template<typename T, typename... Types>
inline int
qvi_bbuff_rmi_unpack(
    void *data,
    T arg,
    Types... args
) {
    byte_t *pos = (byte_t *)data;
    size_t bytes_written;
    int rc = qvi_bbuff_rmi_unpack_item(
        arg,
        (byte_t *)data,
        &bytes_written
    );
    if (rc != QV_SUCCESS) return rc;
    pos += bytes_written;
    return qvi_bbuff_rmi_unpack(pos, args...);
}

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
