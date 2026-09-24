/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2026 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-bbuff.h
 *
 * Byte buffer infrastructure.
 */

#ifndef QVI_BBUFF_H
#define QVI_BBUFF_H

#include "qvi-common.h"
// IWYU pragma: begin_keep
#include "qvi-utils.h"
#include "cereal/archives/binary.hpp"
#include "cereal/types/map.hpp"
#include "cereal/types/memory.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/vector.hpp"
// IWYU pragma: end_keep

/**
 * A growable, contiguous byte buffer used for building and parsing serialized
 * messages (e.g., RPC payloads). Raw bytes can be appended directly with
 * append(), while pack()/unpack() provide typed (de)serialization on top of
 * cereal.
 *
 * Serialization wire format used by pack()/unpack():
 *
 *     [ size_t payload_len ][ payload_len bytes of cereal binary data ]
 *
 * The leading length prefix lets unpack() locate the encoded payload without
 * consuming the rest of the buffer, which is important because a single buffer
 * may contain other bytes (e.g., a message header) preceding the packed region.
 */
struct qvi_bbuff {
private:
    /**
     * Minimum number of bytes by which the backing store grows on a resize.
     * Over-allocating amortizes the cost of repeated small appends.
     */
    static constexpr size_t s_min_growth = QVI_L1_CACHE_LINESIZE;
    /** Number of bytes currently allocated in the backing store. */
    size_t m_capacity = 0;
    /** Number of bytes of valid data currently stored (m_size <= m_capacity). */
    size_t m_size = 0;
    /** Pointer to the heap-allocated backing store (nullptr until m_init()). */
    void *m_data = nullptr;
    /** Allocates the initial backing store and resets size/capacity. */
    void m_init(void)
    {
        // Make sure we get rid of any data that may be present.
        if (m_data) {
            free(m_data);
            m_data = nullptr;
        }
        m_size = 0;
        m_capacity = s_min_growth;
        m_data = calloc(m_capacity, sizeof(byte_t));
        if (qvi_unlikely(!m_data)) throw qvi_runtime_error(QV_ERR_OOR);
    }
public:
    /** Constructs an empty buffer with a freshly allocated backing store. */
    qvi_bbuff(void)
    {
        m_init();
    }
    /** Copy constructor. Performs a deep copy of src's stored bytes. */
    qvi_bbuff(
        const qvi_bbuff &src
    ) {
        *this = src;
    }
    /** Copy-assignment. Performs a deep copy of src's stored bytes. */
    void
    operator=(
        const qvi_bbuff &src
    ) {
        m_init();
        const int rc = append(src.m_data, src.m_size);
        if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error(rc);
    }
    /** Destructor. Frees the backing store. */
    ~qvi_bbuff(void)
    {
        if (m_data) {
            free(m_data);
            m_data = nullptr;
        }
    }
    /** Returns the number of valid data bytes currently stored. */
    size_t
    size(void) const
    {
        return m_size;
    }
    /**
     * Appends size bytes from data to the end of the buffer, growing the
     * backing store if needed. Returns QV_SUCCESS on success or an error code
     * (e.g., QV_ERR_OOR) on allocation failure.
     */
    int
    append(
        const void *const data,
        size_t size
    ) {
        const size_t req_capacity = size + m_size;
        if (req_capacity > m_capacity) {
            // New capacity.
            const size_t new_capacity = req_capacity + s_min_growth;
            void *new_data = calloc(new_capacity, sizeof(byte_t));
            if (qvi_unlikely(!new_data)) return QV_ERR_OOR;
            // Memory allocation successful.
            memmove(new_data, m_data, m_size);
            free(m_data);
            m_capacity = new_capacity;
            m_data = new_data;
        }
        byte_t *dest = static_cast<byte_t *>(m_data);
        dest += m_size;
        memmove(dest, data, size);
        m_size += size;
        return QV_SUCCESS;
    }
    /**
     * Returns a mutable pointer to the start of the contiguous backing store.
     * Valid for size() bytes. The pointer is invalidated by any subsequent
     * operation that may reallocate the buffer (e.g., append(), pack()).
     */
    void *
    data(void)
    {
        return m_data;
    }
    /**
     * Returns a const pointer to the start of the contiguous backing store.
     * Valid for size() bytes. The pointer is invalidated by any subsequent
     * operation that may reallocate the buffer (e.g., append(), pack()).
     */
    const void *
    cdata(void) const
    {
        return m_data;
    }
    /**
     * Serializes the given arguments and appends them to the buffer using the
     * wire format documented on the class: a size_t length prefix followed by
     * the cereal-encoded bytes. Multiple calls append multiple independent
     * length-prefixed regions.
     *
     * @tparam Types Any types serializable by cereal.
     * @param  args  The values to serialize, in order.
     * @return QV_SUCCESS on success, or an error code on allocation/
     *         serialization failure. On failure the buffer contents are
     *         unspecified (a partial region may or may not have been appended).
     */
    template<typename ...Types>
    int
    pack(
        Types &&...args
    ) {
        try {
            std::stringstream ss;
            // Scoped so the archive's destructor flushes its
            // pending output into ss before we read ss.str().
            {
                cereal::BinaryOutputArchive oarchive(ss);
                // Fold expression: serialize each argument in turn.
                (oarchive(std::forward<Types>(args)), ...);
            }
            // Write the size prefix, then the encoded payload.
            const size_t size = ss.view().length();
            const int rc = append(&size, sizeof(size_t));
            if (qvi_unlikely(rc) != QV_SUCCESS) return rc;
            return append(ss.view().cbegin(), size);
        }
        qvi_catch_and_return();
    }

    /**
     * Deserializes values previously written by pack() from a raw buffer,
     * validating the buffer before dereferencing it.
     *
     * This is safe to use on data that may originate from an untrusted or
     * possibly corrupt source (e.g., a message received over the wire): it
     * confirms that data is large enough to hold the length prefix, and that
     * the payload length advertised by the prefix fits within the remaining
     * bytes. This defends against out-of-bounds reads driven by a bogus/oversized
     * length prefix or a truncated payload, both of which are trivial for a
     * malicious or buggy peer to send.
     *
     * @tparam Types     Any types serializable by cereal.
     * @param  data      Pointer to the start of a length-prefixed packed region.
     * @param  data_size Total number of bytes readable at data.
     * @param  args      Output values to deserialize into, in the same order
     *                   they were packed.
     * @return QV_SUCCESS on success; QV_ERR_RPC if the buffer is too small for
     *         the prefix or the advertised payload; or an error code on
     *         deserialization failure.
     */
    template<typename ...Types>
    static int
    unpack(
        void *data,
        size_t data_size,
        Types &&...args
    ) {
        try {
            // Must at least be able to read the length prefix itself.
            if (qvi_unlikely(!data || data_size < sizeof(size_t))) {
                return QV_ERR_RPC;
            }
            byte_t *pos = static_cast<byte_t *>(data);
            // Read the advertised payload length.
            size_t slen;
            memmove(&slen, pos, sizeof(slen));
            pos += sizeof(slen);
            // The advertised payload must fit within the bytes that remain
            // after the prefix; otherwise the buffer is truncated or the
            // prefix is bogus.
            if (qvi_unlikely(slen > data_size - sizeof(size_t))) {
                return QV_ERR_RPC;
            }
            // Ensure the stringstream is opened in binary mode, avoiding
            // potential text-mode translations on some platforms.
            std::stringstream ss(
                std::string(reinterpret_cast<const char *>(pos), slen),
                std::ios_base::in | std::ios_base::binary
            );
            // Scoped so the archive's destructor runs before ss goes away.
            {
                cereal::BinaryInputArchive iarchive(ss);
                iarchive(std::forward<Types>(args)...);
            }
            return QV_SUCCESS;
        }
        qvi_catch_and_return();
    }
};

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
