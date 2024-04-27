/* -*- Mode: C++; c-basic-offset:4; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2020-2024 Triad National Security, LLC
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

#include "qvi-utils.h"

/** Maps return codes to their respective descriptions. */
static const std::map<uint_t, std::string> qvi_rc2str = {
    {QV_SUCCESS, "Success"},
    {QV_SUCCESS_ALREADY_DONE, "Success, operation already complete"},
    {QV_SUCCESS_SHUTDOWN, "Success, shut down"},
    {QV_ERR, "Unspecified error"},
    {QV_ERR_ENV, "Environment error"},
    {QV_ERR_INTERNAL, "Internal error"},
    {QV_ERR_FILE_IO, "File I/O error"},
    {QV_ERR_SYS, "System error"},
    {QV_ERR_OOR, "Out of resources"},
    {QV_ERR_INVLD_ARG, "Invalid argument"},
    {QV_ERR_CALL_BEFORE_INIT, "Call before initialization"},
    {QV_ERR_HWLOC, "Hardware locality error"},
    {QV_ERR_MPI, "MPI error"},
    {QV_ERR_MSG, "Internal message error"},
    {QV_ERR_RPC, "Remote procedure call error"},
    {QV_ERR_NOT_SUPPORTED, "Operation not supported"},
    {QV_ERR_POP, "Pop operation error"},
    {QV_ERR_NOT_FOUND, "Not found"},
    {QV_ERR_SPLIT, "Split error"},
    {QV_RES_UNAVAILABLE, "Resources unavailable"},
    {QV_RC_LAST, ""}
};

const char *
qv_strerr(int ec)
{
    const auto got = qvi_rc2str.find(ec);
    // Not found.
    if (got == qvi_rc2str.end()) {
        static const cstr_t bad = "";
        return bad;
    }
    return got->second.c_str();
}

cstr_t
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

bool
qvi_path_usable(
    const cstr_t path,
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
    cstr_t str,
    int *maybe_val
) {
    *maybe_val = 0;

    cstr_t tstr = str;
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
    const cstr_t ports = getenv(QVI_ENV_PORT);
    if (!ports) return QV_ERR_ENV;
    return qvi_atoi(ports, port);
}

int
qvi_url(
    char **url
) {
    static const cstr_t base = "tcp://127.0.0.1";

    int port = 0;
    int rc = qvi_port(&port);
    if (rc != QV_SUCCESS) return rc;

    int nw = asprintf(url, "%s:%d", base, port);
    if (nw == -1) return QV_ERR_OOR;

    return QV_SUCCESS;
}

cstr_t
qvi_conn_ers(void)
{
    static const cstr_t msg = "Cannot determine connection information. "
                              "Please make sure that the following environment "
                              "variable is set to an unused port number: "
                              QVI_ENV_PORT;
    return msg;
}

cstr_t
qvi_tmpdir(void)
{
    static thread_local char tmpdir[PATH_MAX];

    cstr_t qvenv = getenv(QVI_ENV_TMPDIR);
    if (qvenv) {
        int nw = snprintf(tmpdir, PATH_MAX, "%s", qvenv);
        if (nw < PATH_MAX) return tmpdir;
    }
    qvenv = getenv("TMPDIR");
    if (qvenv) {
        int nw = snprintf(tmpdir, PATH_MAX, "%s", qvenv);
        if (nw < PATH_MAX) return tmpdir;
    }
    static cstr_t tmp = "/tmp";
    return tmp;
}

cstr_t
qvi_whoami(void)
{
    static const int bsize = 128;
    static thread_local char buff[bsize];
    cstr_t user = getenv("USER");
    if (!user) {
        user = PACKAGE_NAME;
    }
    const int nw = snprintf(buff, bsize, "%s", user);
    if (nw >= bsize) return PACKAGE_NAME;
    return buff;
}

int
qvi_file_size(
    const cstr_t path,
    size_t *size
) {
    struct stat st;
    const int rc = stat(path, &st);
    if (rc == -1) return QV_ERR_FILE_IO;
    *size = st.st_size;
    return QV_SUCCESS;
}

int64_t
qvi_cantor_pairing(
    int a,
    int b
) {
    return (a + b) * (a + b + 1) / 2 + b;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
