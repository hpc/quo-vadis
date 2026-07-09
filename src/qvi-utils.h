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
 * Returns whether the provided environment variable is set.
 */
bool
qvi_envset(
    const std::string &varname
) noexcept;

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
    T *copy
) {
    try {
        *copy = t;
        return QV_SUCCESS;
    }
    qvi_catch_and_return();
}

/**
 * See gettid(2) for details.
 */
static inline pid_t
qvi_gettid(void)
{
    return (pid_t)syscall(SYS_gettid);
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
    int base = 10
);

std::vector<std::string>
qvi_split_string(
    const std::string &str,
    const std::string &delimiter
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

int
qvi_port_from_env(void);

/**
 * Cantor pairing function.
 */
int64_t
qvi_cantor_pairing(
    int a,
    int b
);

int
qvi_session_discover(
    uint_t max_timeout_in_ms,
    int &portno
);

/**
 *
 */
int
qvi_start_quo_vadisd(
    int portno
);

/**
 * Returns a vector of PIDs that match the provided process name.
 */
int
qvi_running(
    const std::string &name,
    std::vector<pid_t> &pids
);

/**
 * Returns a vector containing the command line arguments
 * passed to the process identified by the provided PID.
 */
int
qvi_pid_cmdline(
    pid_t pid,
    std::vector<std::string> &argv
);

/**
 * Returns a vector of environment variable pairs
 * used when the provided PID was started.
 */
int
qvi_pid_environ(
    pid_t pid,
    std::vector<std::string> &envs
);

/**
 * Returns the largest number that will fit in the space available.
 */
size_t
qvi_maxfit(
    size_t max_chunk,
    size_t space_left
);

/**
 * Returns the max i per k.
 */
size_t
qvi_maxiperk(
    size_t i,
    size_t k
);

/**
 * Splits the provided vector into npieces that are as close to equal size as
 * possible given the size of the input vector and the number of pieces
 * requested. The contents of the resulting vectors are copies of the contents
 * of the input vector.
 */
template <typename T>
std::vector<std::vector<T>>
qvi_vector_split(
    const std::vector<T> &vec,
    size_t npieces
) {
    const size_t ntotal = vec.size();
    std::vector<std::vector<T>> result;
    // An empty split.
    if (qvi_unlikely(npieces == 0 || ntotal == 0 || npieces > ntotal)) {
        return result;
    }

    const size_t base_chunk_size = ntotal / npieces;
    const size_t remainder = ntotal % npieces;

    auto current_it = vec.begin();
    for (size_t i = 0; i < npieces; ++i) {
        const size_t chunk_size = base_chunk_size + (i < remainder ? 1 : 0);
        // Use vector range constructor to copy elements.
        result.emplace_back(current_it, std::next(current_it, chunk_size));
        std::advance(current_it, chunk_size);
    }
    return result;
}

/**
 * Implements Min-Cost Max-Flow using SPFA (Shortest Path Faster Algorithm).
 */
struct qvi_min_cost_max_flow {
private:
    struct edge {
        int to, rev;
        int64_t cap, cost, flow;
    };

    int n;
    std::vector<std::vector<edge>> graph;
    std::vector<int64_t> dist;
    std::vector<int> parent, parent_edge;

    bool
    spfa(
        int s,
        int t
    ) {
        dist.assign(n, std::numeric_limits<int64_t>::max());
        parent.assign(n, -1);
        parent_edge.assign(n, -1);
        std::vector<bool> in_queue(n, false);

        std::queue<int> q;
        dist[s] = 0;
        q.push(s);
        in_queue[s] = true;

        while (!q.empty()) {
            const int u = q.front();
            q.pop();
            in_queue[u] = false;

            for (size_t i = 0; i < graph[u].size(); ++i) {
                const edge &e = graph[u][i];
                if (e.cap > 0 && dist[u] + e.cost < dist[e.to]) {
                    dist[e.to] = dist[u] + e.cost;
                    parent[e.to] = u;
                    parent_edge[e.to] = i;
                    if (!in_queue[e.to]) {
                        q.push(e.to);
                        in_queue[e.to] = true;
                    }
                }
            }
        }
        return dist[t] != std::numeric_limits<int64_t>::max();
    }

public:
    qvi_min_cost_max_flow(
        int n
    ) : n(n)
      , graph(n)
      , dist(n)
      , parent(n)
      , parent_edge(n) { }

    void
    add_edge(
        int from,
        int to,
        int64_t cap,
        int64_t cost
    ) {
        graph[from].push_back({to, (int)graph[to].size(), cap, cost, 0});
        graph[to].push_back({from, (int)graph[from].size() - 1, 0, -cost, 0});
    }

    std::pair<int64_t, int64_t>
    min_cost_flow(
        int s,
        int t,
        int64_t max_flow
    ) {
        int64_t flow = 0, cost = 0;

        while (flow < max_flow && spfa(s, t)) {
            int64_t push = max_flow - flow;
            int v = t;
            while (v != s) {
                const int u = parent[v];
                const int edge_idx = parent_edge[v];
                push = std::min(push, graph[u][edge_idx].cap);
                v = u;
            }

            v = t;
            while (v != s) {
                const int u = parent[v];
                const int edge_idx = parent_edge[v];
                graph[u][edge_idx].cap -= push;
                graph[u][edge_idx].flow += push;
                graph[v][graph[u][edge_idx].rev].cap += push;
                graph[v][graph[u][edge_idx].rev].flow -= push;
                v = u;
            }
            flow += push;
            cost += push * dist[t];
        }
        return {flow, cost};
    }

    const std::vector<std::vector<edge>> &
    get_graph(void) const {
        return graph;
    }
};

#endif

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
