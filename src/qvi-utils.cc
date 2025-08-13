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

int
qvi_port_from_env(
    int &portno
) {
    int rc = QV_SUCCESS;
    do {
        const cstr_t ports = getenv(QVI_ENV_PORT.c_str());
        if (!ports) {
            rc = QV_ERR_ENV;
            break;
        }
        rc = qvi_stoi(std::string(ports), portno);
        if (qvi_unlikely(rc != QV_SUCCESS)) {
            rc = QV_ERR_ENV;
            break;
        }
    } while (false);
    if (qvi_unlikely(rc != QV_SUCCESS)) {
        portno = QVI_PORT_UNSET;
    }
    return rc;
}

int64_t
qvi_cantor_pairing(
    int a,
    int b
) {
    return (a + b) * ((a + b + 1) / 2) + b;
}

static const std::regex &
get_session_regex(void)
{
    static const std::regex session_re("quo-vadisd\\.([0-9]+)");
    return session_re;
}

/**
 * Attempts discovery until success or max delay is reached.
 */
static int
discover_with_backoff(
    std::function<int(int &)> discover_fn,
    int &portno,
    uint_t max_delay_in_ms
) {
    namespace sc = std::chrono;

    const uint_t start_delay = 1;
    if (max_delay_in_ms < start_delay) {
        max_delay_in_ms = start_delay;
    }
    // Initial delay.
    sc::milliseconds cur_delay(start_delay);
    const sc::milliseconds max_delay(max_delay_in_ms);
    // Jitter random number generator.
    std::mt19937 jitter_rng(std::random_device{}());

    do {
        const int rc = discover_fn(portno);
        if (rc == QV_SUCCESS) return rc;
        else {
            // Calculate jitter: e.g., 0% to 50% of current_delay.
            std::uniform_int_distribution<int> jdist(0, cur_delay.count() / 2);
            const sc::milliseconds jitter = sc::milliseconds(jdist(jitter_rng));
            std::this_thread::sleep_for(cur_delay + jitter);
            // Double the delay for the next attempt, up to max_delay.
            cur_delay = std::min(cur_delay * 2, max_delay);
        }
    } while (cur_delay != max_delay);
    // No port found, so unset it.
    portno = QVI_PORT_UNSET;
    return QV_ERR;
}

static int
discover_impl(
    int &portno
) {
    namespace fs = std::filesystem;

    const std::string tmpdir = qvi_tmpdir();
    const std::regex &session_regex = get_session_regex();

    try {
        std::smatch match;
        for (const auto &entry : fs::directory_iterator(tmpdir)) {
            if (fs::is_directory(entry.path())) {
                const std::string dirname = entry.path().filename().string();
                // Found the session directory.
                if (std::regex_search(dirname, match, session_regex)) {
                    return qvi_stoi(match[1].str(), portno);
                }
            }
        }
    }
    catch (const fs::filesystem_error &e) {
        qvi_log_error(e.what());
        return QV_ERR_FILE_IO;
    }
    return QV_ERR_NOT_FOUND;
}

bool
qvi_session_exists(
    int portno
) {
    const std::string tmpdir = qvi_tmpdir();
    int e;
    return qvi_access(
        qvi_tmpdir() + "/quo-vadisd." + std::to_string(portno), R_OK | W_OK, &e
    );
}

int
qvi_session_discover(
    uint_t max_timeout_in_ms,
    int &portno
) {
    // Account for potential delays in publishing session
    // information between server startup and client discovery.
    return discover_with_backoff(discover_impl, portno, max_timeout_in_ms);
}

int
qvi_start_qvd(
    int portno
) {
    const pid_t pid = fork();
    // Fork failed.
    if (qvi_unlikely(pid == -1)) {
        qvi_log_error(
            "\n\n#############################################\n"
            "# fork() failed while starting quo-vadisd"
            "\n#############################################\n\n"
        );
        return QV_ERR_SYS;
    }
    // Child
    else if (pid == 0) {
        std::vector<std::string> argss = {
            "quo-vadisd",
            "--port",
            std::to_string(portno)
        };

        std::vector<char *> argv;
        for (const auto &arg : argss) {
            argv.push_back(const_cast<char *>(arg.c_str()));
        }
        argv.push_back(nullptr);
        // Exec the daemon.
        execvp(argv[0], argv.data());
        // If we get here, then the command failed.
        qvi_log_error(
            "\n\n#############################################\n"
            "# Could not start quo-vadisd.\n#\n"
            "# For automatic startup, ensure the path to\n"
            "# quo-vadisd is in your PATH and try again.\n"
            "# For manual startup, ensure quo-vadisd is\n"
            "# running across all the servers in your job."
            "\n#############################################\n\n"
        );
        _exit(EXIT_FAILURE);
    }
    return QV_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
