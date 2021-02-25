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
 * @file qvi-utils.cc
 */

#include "qvi-common.h"
#include "qvi-utils.h"

/** Description of the return codes. */
static const char *qvi_rc_strerrs[] = {
    "Success",
    "Success, operation already complete",
    "Unspecified error",
    "Environment error",
    "Internal error",
    "File I/O error",
    "System error",
    "Out of resources",
    "Invalid argument",
    "Call before initialization",
    "Hardware locality error",
    "MPI error",
    "Internal message error",
    "Remote procedure call error",
    "Operation not supported",
    "Pop operation error",
    "PMI operation error",
    "Not found"
};

const char *
qv_strerr(int ec)
{
    if (ec < 0 || ec >= QV_RC_LAST) {
        static const cstr bad = "";
        return bad;
    }
    return qvi_rc_strerrs[ec];
}

char *
qvi_strerr(int ec)
{
    static thread_local char sb[1024];
    return strerror_r(ec, sb, sizeof(sb));
}

pid_t
qvi_gettid(void) {
    return (pid_t)syscall(SYS_gettid);
}

double
qvi_time(void)
{
    using namespace std::chrono;

    const auto n = steady_clock::now();
    auto tse_ms = time_point_cast<microseconds>(n).time_since_epoch().count();
    return double(tse_ms) / 1e6;
}

struct qvi_byte_buffer_s {
    // Current capacity of buffer.
    size_t capacity = 0;
    // Amount of data already stored.
    size_t size = 0;
    // Pointer to data backing store.
    void *data = nullptr;
    // Buffer constants.
    enum constants {
        // Minimum growth for resizes, etc.
        min_growth = 256
    };
};

int
qvi_byte_buffer_construct(
    qvi_byte_buffer_t **buff
) {
    int rc = QV_SUCCESS;

    qvi_byte_buffer_t *ibuff = qvi_new qvi_byte_buffer_t;
    if (!ibuff) {
        rc = QV_ERR_OOR;
        goto out;
    }

    ibuff->capacity = ibuff->min_growth;
    ibuff->data = calloc(ibuff->capacity, sizeof(uint8_t));
    if (!ibuff->data) {
        rc = QV_ERR_OOR;
        goto out;
    }
out:
    if (rc != QV_SUCCESS) {
        qvi_byte_buffer_destruct(&ibuff);
    }
    *buff = ibuff;
    return rc;
}

void
qvi_byte_buffer_destruct(
    qvi_byte_buffer_t **buff
) {
    qvi_byte_buffer_t *ibuff = *buff;
    if (!ibuff) return;
    if (ibuff->data) free(ibuff->data);
    delete ibuff;
    *buff = nullptr;
}

void *
qvi_byte_buffer_data(
    qvi_byte_buffer_t *buff
) {
    return buff->data;
}

size_t
qvi_byte_buffer_size(
    const qvi_byte_buffer_t *buff
) {
    return buff->size;
}

int
qvi_byte_buffer_append(
    qvi_byte_buffer_t *buff,
    void *data,
    size_t size
) {
    const size_t req_capacity = size + buff->size;
    if (req_capacity > buff->capacity) {
        // New capacity.
        const size_t new_capacity = req_capacity + buff->min_growth;
        void *new_data = calloc(new_capacity, sizeof(uint8_t));
        if (!new_data) return QV_ERR_OOR;
        // Memory allocation successful.
        memcpy(new_data, buff->data, buff->size);
        free(buff->data);
        buff->capacity = new_capacity;
        buff->data = new_data;
    }
    uint8_t *dest = (uint8_t *)buff->data;
    dest += buff->size + 1;
    memcpy(dest, data, size);
    buff->size += size;
    return QV_SUCCESS;
}

bool
qvi_path_usable(
    const char *path,
    int *errc
) {
    *errc = 0;
    if (access(path, R_OK | W_OK) == -1) {
        *errc = errno;
        return false;
    }
    return true;
}

int
qvi_atoi(
    const char *str,
    int *maybe_val
) {
    cstr tstr = str;
    // Make certain str contrains only digits.
    while ('\0' != *tstr) {
        if (!isdigit(*tstr)) return QV_ERR_INVLD_ARG;
        ++tstr;
    }
    errno = 0;
    char *end = nullptr;
    long val = strtol(str, &end, 10);
    int err = errno;
    // Did we get any digits?
    if (str == end) return QV_ERR_INVLD_ARG;
    // In valid range?
    if (val > INT_MAX || (err == ERANGE && val == LONG_MAX)) {
        return QV_ERR_INVLD_ARG;
    }
    if (val < INT_MIN || (err == ERANGE && val == LONG_MIN)) {
        return QV_ERR_INVLD_ARG;
    }
    if (*end != '\0') return QV_ERR_INVLD_ARG;

    *maybe_val = (int)val;
    return QV_SUCCESS;
}

int
qvi_port(
    int *port
) {
    cstr ports = getenv(QVI_ENV_PORT);
    if (!ports) return QV_ERR_ENV;
    return qvi_atoi(ports, port);
}

int
qvi_url(
    char **url
) {
    static const cstr base = "tcp://127.0.0.1";

    int port;
    int rc = qvi_port(&port);
    if (rc != QV_SUCCESS) return rc;

    int nw = asprintf(url, "%s:%d", base, port);
    if (nw == -1) return QV_ERR_OOR;

    return QV_SUCCESS;
}

const char *
qvi_conn_ers(void)
{
    static const char *msg = "Cannot determine connection information. "
                             "Please make sure that the following environment "
                             "variable is set to an unused port number: "
                             QVI_ENV_PORT;
    return msg;
}

const char *
qvi_tmpdir(void)
{
    static thread_local char tmpdir[PATH_MAX];

    cstr qvenv = getenv(QVI_ENV_TMPDIR);
    if (qvenv) {
        int nw = snprintf(tmpdir, PATH_MAX, "%s", qvenv);
        if (nw < PATH_MAX) return tmpdir;
    }
    qvenv = getenv("TMPDIR");
    if (qvenv) {
        int nw = snprintf(tmpdir, PATH_MAX, "%s", qvenv);
        if (nw < PATH_MAX) return tmpdir;
    }
    static cstr tmp = "/tmp";
    return tmp;
}

const char *
whoami(void)
{
    static const int bsize = 128;
    static thread_local char buff[bsize];
    cstr user = getenv("USER");
    if (!user) {
        user = PACKAGE_NAME;
    }
    int nw = snprintf(buff, bsize, "%s", user);
    if (nw >= bsize) return PACKAGE_NAME;
    return buff;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
