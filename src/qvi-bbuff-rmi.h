/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2021-2024 Triad National Security, LLC
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
 * a = size_t
 * b = qvi_bbuff_rmi_bytes_in_t, qvi_bbuff_rmi_bytes_out_t
 * c = hwloc_cpuset_t
 * c = qvi_hwloc_bitmap_s
 * h = qvi_hwpool_s *
 * h = qvi_line_hwpool_t *
 * i = int
 * s = char *
 * s = std::string
 * t = qvi_task_id_t
 * z = qvi_bbuff_rmi_zero_msg_t
 */

#ifndef QVI_BBUFF_RMI_H
#define QVI_BBUFF_RMI_H

#include "qvi-common.h"
#include "qvi-hwloc.h"
#include "qvi-hwpool.h"

// 'Null' cpuset representation as a string.
static constexpr cstr_t QV_BUFF_RMI_NULL_CPUSET = "";

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
    size_t
) {
    picture += "a";
}

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
    const qvi_hwloc_bitmap_s &
) {
    picture += "c";
}

template<>
inline void
qvi_bbuff_rmi_pack_type_picture(
    std::string &picture,
    const qvi_hwpool_s *
) {
    picture += "h";
}

template<>
inline void
qvi_bbuff_rmi_pack_type_picture(
    std::string &picture,
    qvi_hwpool_s *
) {
    picture += "h";
}

template<>
inline void
qvi_bbuff_rmi_pack_type_picture(
    std::string &picture,
    qvi_hwpool_s **
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

template<>
inline void
qvi_bbuff_rmi_pack_type_picture(
    std::string &picture,
    qvi_task_type_t
) {
    picture += "i";
}

template<>
inline void
qvi_bbuff_rmi_pack_type_picture(
    std::string &picture,
    qvi_task_type_t *
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
    std::string
) {
    picture += "s";
}

template<>
inline void
qvi_bbuff_rmi_pack_type_picture(
    std::string &picture,
    std::string &
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
    T&& arg,
    Types&&... args
) {
    qvi_bbuff_rmi_pack_type_picture(picture, std::forward<T>(arg));
    qvi_bbuff_rmi_get_picture(picture, std::forward<Types>(args)...);
}

template<typename T>
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff_t *,
    T
);

/**
 * Packs size_t.
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff_t *buff,
    size_t data
) {
    return qvi_bbuff_append(buff, &data, sizeof(data));
}

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
 * Packs qvi_task_type_t as an int.
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff_t *buff,
    qvi_task_type_t data
) {
    const int dai = (int)data;
    return qvi_bbuff_append(buff, &dai, sizeof(dai));
}

inline int
qvi_bbuff_rmi_pack_item_impl(
    qvi_bbuff_t *buff,
    cstr_t data
) {
    return qvi_bbuff_append(buff, data, strlen(data) + 1);
}

/**
 * Packs std::string
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff_t *buff,
    const std::string &data
) {
    return qvi_bbuff_rmi_pack_item_impl(buff, data.c_str());
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
 * Packs qvi_hwloc_bitmap_s
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff_t *buff,
    const qvi_hwloc_bitmap_s &bitmap
) {
    return qvi_bbuff_rmi_pack_item_impl(buff, bitmap.data);
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
 * Packs qvi_hwpool_dev_s *
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff_t *buff,
    qvi_hwpool_dev_s *data
) {
    // Pack device affinity.
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
 * Packs qvi_hwpool_s *
 */
inline int
qvi_bbuff_rmi_pack_item_impl(
    qvi_bbuff_t *buff,
    const qvi_hwpool_s *data
) {
    // Pack cpuset.
    // TODO(skg) Add flags.
    // TODO(skg) Pack cpu
    int rc = qvi_bbuff_rmi_pack_item(buff, data->cpu.cpuset);
    if (rc != QV_SUCCESS) return rc;
    // Pack ndevinfos
    const size_t ndev = data->devs.size();
    rc = qvi_bbuff_rmi_pack_item(buff, ndev);
    if (rc != QV_SUCCESS) return rc;
    // Pack devices.
    for (const auto &dev : data->devs) {
        qvi_bbuff_rmi_pack_item(buff, dev.second.get());
    }
    return rc;
}

/**
 * Packs qvi_task_id_t
 */
inline int
qvi_bbuff_rmi_pack_item_impl(
    qvi_bbuff_t *buff,
    qvi_task_id_t data
) {
    // Pack task type.
    int rc = qvi_bbuff_rmi_pack_item(buff, data.type);
    if (rc != QV_SUCCESS) return rc;
    // Pack pid
    return qvi_bbuff_rmi_pack_item(buff, data.who);
}

/**
 * Packs const qvi_hwpool_s *
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff_t *buff,
    const qvi_hwpool_s *data
) {
    return qvi_bbuff_rmi_pack_item_impl(buff, data);
}

/**
 * Packs qvi_hwpool_s *
 */
inline int
qvi_bbuff_rmi_pack_item(
    qvi_bbuff_t *buff,
    qvi_hwpool_s *data
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
    T&& arg,
    Types&&... args
) {
    const int rc = qvi_bbuff_rmi_pack_item(buff, std::forward<T>(arg));
    if (rc != QV_SUCCESS) return rc;
    return qvi_bbuff_rmi_pack(buff, std::forward<Types>(args)...);
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
    size_t *i,
    byte_t *buffpos,
    size_t *bytes_written
) {
    memmove(i, buffpos, sizeof(*i));
    *bytes_written = sizeof(*i);
    return QV_SUCCESS;
}

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
    qvi_task_type_t *o,
    byte_t *buffpos,
    size_t *bytes_written
) {
    // Remember we are sending this as an int.
    int oai = 0;
    memmove(&oai, buffpos, sizeof(oai));
    *bytes_written = sizeof(oai);
    *o = (qvi_task_type_t)oai;
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
    std::string &str,
    byte_t *buffpos,
    size_t *bytes_written
) {
    str = std::string((const char *)buffpos);
    *bytes_written = strlen((char *)buffpos) + 1;
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
        qvi_hwloc_bitmap_free(cpuset);
    }
    return rc;
}

inline int
qvi_bbuff_rmi_unpack_item(
    qvi_hwloc_bitmap_s &bitmap,
    byte_t *buffpos,
    size_t *bytes_written
) {
    hwloc_cpuset_t raw_cpuset = nullptr;
    int rc = qvi_bbuff_rmi_unpack_item(
        &raw_cpuset, buffpos, bytes_written
    );
    if (rc != QV_SUCCESS) return rc;

    rc = qvi_hwloc_bitmap_copy(raw_cpuset, bitmap.data);
    qvi_hwloc_bitmap_free(&raw_cpuset);
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

/**
 * Unpacks qvi_hwpool_dev_s *
 */
inline int
qvi_bbuff_rmi_unpack_item(
    qvi_hwpool_dev_s *dev,
    byte_t *buffpos,
    size_t *bytes_written
) {
    size_t bw = 0, total_bw = 0;

    int rc = qvi_bbuff_rmi_unpack_item(
        dev->affinity, buffpos, &bw
    );
    if (rc != QV_SUCCESS) goto out;
    total_bw += bw;
    buffpos += bw;

    rc = qvi_bbuff_rmi_unpack_item(
        &dev->type, buffpos, &bw
    );
    if (rc != QV_SUCCESS) goto out;
    total_bw += bw;
    buffpos += bw;

    rc = qvi_bbuff_rmi_unpack_item(
        &dev->id, buffpos, &bw
    );
    if (rc != QV_SUCCESS) goto out;
    total_bw += bw;
    buffpos += bw;

    rc = qvi_bbuff_rmi_unpack_item(
        dev->pci_bus_id, buffpos, &bw
    );
    if (rc != QV_SUCCESS) goto out;
    total_bw += bw;
    buffpos += bw;

    rc = qvi_bbuff_rmi_unpack_item(
        dev->uuid, buffpos, &bw
    );
    if (rc != QV_SUCCESS) goto out;
    total_bw += bw;
out:
    if (rc != QV_SUCCESS) {
        total_bw = 0;
    }
    *bytes_written = total_bw;
    return rc;
}

/**
 * Unpacks qvi_hwpool_s **
 */
inline int
qvi_bbuff_rmi_unpack_item(
    qvi_hwpool_s **hwp,
    byte_t *buffpos,
    size_t *bytes_written
) {
    qvi_hwpool_s *ihwp = nullptr;
    int rc = qvi_hwpool_new(&ihwp);
    if (rc != QV_SUCCESS) return rc;

    size_t bw = 0, total_bw = 0;

    // Unpack cpuset.
    rc = qvi_bbuff_rmi_unpack_item(
        ihwp->cpu.cpuset, buffpos, &bw
    );
    if (rc != QV_SUCCESS) return rc;
    total_bw += bw;
    buffpos += bw;
    // Unpack number of devinfos.
    size_t ndev = 0;
    rc = qvi_bbuff_rmi_unpack_item(
        &ndev, buffpos, &bw
    );
    if (rc != QV_SUCCESS) goto out;
    total_bw += bw;
    buffpos += bw;
    // Unpack devices.
    for (size_t i = 0; i < ndev; ++i) {
        qvi_hwpool_dev_s dev;
        rc = qvi_bbuff_rmi_unpack_item(
            &dev, buffpos, &bw
        );
        total_bw += bw;
        buffpos += bw;
        if (rc != QV_SUCCESS) break;

        rc = ihwp->add_device(dev);
        if (rc != QV_SUCCESS) break;
    }
out:
    if (rc != QV_SUCCESS) {
        total_bw = 0;
    }
    *bytes_written = total_bw;
    *hwp = ihwp;
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
    T&& arg,
    Types&&... args
) {
    byte_t *pos = (byte_t *)data;
    size_t bytes_written = 0;
    const int rc = qvi_bbuff_rmi_unpack_item(
        std::forward<T>(arg),
        (byte_t *)data,
        &bytes_written
    );
    if (rc != QV_SUCCESS) return rc;
    pos += bytes_written;
    return qvi_bbuff_rmi_unpack(pos, std::forward<Types>(args)...);
}

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
