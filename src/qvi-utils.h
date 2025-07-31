/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2025 Triad National Security, LLC
 *                         All rights reserved.
 *
 * Copyright (c) 2020-2021 Lawrence Livermore National Security, LLC
 *                         All rights reserved.
 *
 * This file is part of the quo-vadis project. See the LICENSE file at the
 * top-level directory of this distribution.
 */

/**
 * @file qvi-utils.h
 */

#ifndef QVI_UTILS_H
#define QVI_UTILS_H

#include "qvi-common.h" // IWYU pragma: keep

/**
 * Reference counting base class that provides retain/release semantics.
 */
struct qvi_refc {
private:
    mutable std::atomic<int64_t> refc = {1};
public:
    virtual ~qvi_refc(void) = default;

    void
    retain(void) const
    {
        ++refc;
    }

    void
    release(void) const
    {
        assert(refc > 0);
        if (--refc == 0) {
            delete this;
        }
    }
};

/**
 * Implements a fixed-capacity least recently used cache.
 */
template <typename Key, typename Value>
struct qvi_lru_cache {
private:
    size_t m_capacity;
    std::list<std::pair<Key, Value>> m_cache_list;
    std::unordered_map<
        Key,
        typename std::list<std::pair<Key, Value>
    >::iterator> m_cache_map;
public:
    /** Constructor. */
    qvi_lru_cache(
        size_t capacity
    ) : m_capacity(capacity) { }

    /**
     * Stores a key, value pair.
     */
    void
    put(
        const Key &key,
        const Value &value
    ) {
        auto got = m_cache_map.find(key);
        if (got != m_cache_map.end()) {
            // Update value and move to front.
            got->second->second = value;
            m_cache_list.splice(
                m_cache_list.begin(), m_cache_list, got->second
            );
            return;
        }
        // If cache is full, remove the last element.
        if (m_cache_list.size() == m_capacity) {
            m_cache_map.erase(m_cache_list.back().first);
            m_cache_list.pop_back();
        }
        // Add new element to the front.
        m_cache_list.emplace_front(key, value);
        m_cache_map[key] = m_cache_list.begin();
    }

    /**
     * Attempts to get a cached value based on the provided key. If found,
     * QV_SUCCESS is returned and value is valid. If not found, QV_ERR_NOT_FOUND
     * is returned and value is invalid.
     */
    int
    get(
        const Key &key,
        Value &value
    ) {
        value = {};

        auto it = m_cache_map.find(key);
        if (it == m_cache_map.end()) {
            return QV_ERR_NOT_FOUND;
        }
        // Move the accessed node to the front of the list.
        m_cache_list.splice(m_cache_list.begin(), m_cache_list, it->second);
        value = it->second->second;
        return QV_SUCCESS;
    }
};

/**
 * Constructs a new object of a given type. *t will be valid if successful,
 * undefined otherwise. Returns QV_SUCCESS if successful.
 */
template <class T, typename... Types>
int
qvi_new(
    T **t,
    Types&&... args
) {
    try {
        *t = new T(std::forward<Types>(args)...);
        return QV_SUCCESS;
    }
    qvi_catch_and_return();
}

/**
 * Simple wrapper around delete that also nullifies the input pointer.
 */
template <class T>
void
qvi_delete(
    T **t
) {
    if (qvi_unlikely(!t)) return;
    T *it = *t;
    if (it) {
        delete it;
    }
    *t = nullptr;
}

/**
 * Simple wrapper that duplicates the provided instance.
 */
template <class T>
int
qvi_dup(
    const T &t,
    T **dup
) {
    try {
        *dup = new T(t);
        return QV_SUCCESS;
    }
    qvi_catch_and_return();
}

/**
 * Simple wrapper that copies the provided instance.
 */
template <class T>
int
qvi_copy(
    const T &t,
    T *dup
) {
    try {
        *dup = t;
        return QV_SUCCESS;
    }
    qvi_catch_and_return();
}

/**
 *
 */
int
qvi_start_qvd(void);

/**
 * See gettid(2) for details.
 */
static inline pid_t
qvi_gettid(void)
{
    return gettid();
}

/**
 *
 */
double
qvi_time(void);

/**
 *
 */
bool
qvi_access(
    const std::string &path,
    int mode,
    int *errc
);

/**
 * Converts string to an int, if possible.
 */
int
qvi_stoi(
    const std::string &str,
    int &maybe_result,
    int base = 10
);

/**
 * Removes the provided path, including all its contents.
 */
int
qvi_rmall(
    const std::string &path
);

/**
 *
 */
std::string
qvi_tmpdir(void);

/**
 *
 */
int
qvi_file_size(
    const std::string &path,
    size_t *size
);

/**
 * Cantor pairing function.
 */
int64_t
qvi_cantor_pairing(
    int a,
    int b
);

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
