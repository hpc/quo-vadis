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
    {QV_ERR_HWLOC, "Hardware locality error"},
    {QV_ERR_MPI, "MPI error"},
    {QV_ERR_MSG, "Internal message error"},
    {QV_ERR_RPC, "Remote procedure call error"},
    {QV_ERR_NOT_SUPPORTED, "Operation not supported"},
    {QV_ERR_NOT_FOUND, "Not found"},
    {QV_ERR_SPLIT, "Split error"},
    {QV_RES_UNAVAILABLE, "Resources unavailable"}
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

bool
qvi_envset(
    const std::string &varname
) noexcept {
    return (getenv(varname.c_str()) != nullptr);
}

double
qvi_time(void)
{
    using namespace std::chrono;

    const auto n = steady_clock::now();
    const auto m = time_point_cast<microseconds>(n).time_since_epoch().count();
    return double(m) / 1e6;
}

bool
qvi_access(
    const std::string &path,
    int mode,
    int *errc
) {
    *errc = 0;
    if (access(path.c_str(), mode) == -1) {
        *errc = errno;
        return false;
    }
    return true;
}

int
qvi_stoi(
    const std::string &str,
    int &maybe_result,
    int base
) {
    try {
        maybe_result = std::stoi(str, nullptr, base);
        return QV_SUCCESS;
    }
    catch (const std::invalid_argument &e) { }
    catch (const std::out_of_range &e) { }
    maybe_result = 0;
    return QV_ERR_INVLD_ARG;
}

static int
rmall_cb(
    const char *path,
    const struct stat *,
    int,
    struct FTW *
) {
    errno = 0;
    const int rc = remove(path);
    if (qvi_unlikely(rc != 0)) {
        const int eno = errno;
        qvi_log_error(
            "remove({}) failed with errno={} ({})",
            path, eno, strerror(eno)
        );
    }
    return rc;
}

int
qvi_rmall(
    const std::string &path
) {
    const int rc = nftw(path.c_str(), rmall_cb, 64, FTW_DEPTH | FTW_PHYS);
    if (qvi_unlikely(rc != 0)) return QV_ERR_FILE_IO;
    return QV_SUCCESS;
}

std::string
qvi_tmpdir(void)
{
    cstr_t qvenv = getenv(QVI_ENV_TMPDIR.c_str());
    if (qvenv) return std::string(qvenv);

    qvenv = getenv("TMPDIR");
    if (qvenv) return std::string(qvenv);

    return std::string("/tmp");
}

int
qvi_file_size(
    const std::string &path,
    size_t *size
) {
    struct stat st;
    const int rc = stat(path.c_str(), &st);
    if (qvi_unlikely(rc == -1)) return QV_ERR_FILE_IO;
    *size = st.st_size;
    return QV_SUCCESS;
}

int64_t
qvi_cantor_pairing(
    int a,
    int b
) {
    return (a + b) * ((a + b + 1) / 2) + b;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
