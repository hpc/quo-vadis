/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2021-2025 Triad National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-bbuff-rmi.h
 */

#ifndef QVI_BBUFF_RMI_H
#define QVI_BBUFF_RMI_H

#include "qvi-common.h"
#include "qvi-bbuff.h" // IWYU pragma: keep

#include "cereal/types/memory.hpp"
#include "cereal/types/map.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/archives/binary.hpp"

// 'Null' cpuset representation as a string.
static constexpr cstr_t QV_BUFF_RMI_NULL_CPUSET = "";

// Defines a zero message type.
typedef enum qvi_bbuff_rmi_zero_msg_e {
    QVI_BBUFF_RMI_ZERO_MSG = 0
} qvi_bbuff_rmi_zero_msg_t;

template<typename... Types>
inline int
qvi_bbuff_rmi_pack(
    qvi_bbuff *buff,
    Types &&...args
) {
    std::stringstream ss;
    {
        cereal::BinaryOutputArchive oarchive(ss);
        // Use a fold expression to serialize each argument
        (oarchive(std::forward<Types>(args)), ...);
    }

    const std::string sdata(ss.str());
    size_t len = sdata.length();

    buff->append(&len, sizeof(size_t));
    return buff->append(sdata.data(), sdata.size());
}

template<typename... Types>
inline int
qvi_bbuff_rmi_unpack(
    void *data,
    Types &&...args
) {
    byte_t *pos = (byte_t *)data;

    size_t slen;
    memmove(&slen, pos, sizeof(slen));
    pos += sizeof(slen);

    std::string str = std::string((const char *)pos, slen);

    std::stringstream ss(str);
    {
        cereal::BinaryInputArchive iarchive(ss);
        // Use a fold expression to serialize each argument
        (iarchive(std::forward<Types>(args)), ...);
    }

    return QV_SUCCESS;
}

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
