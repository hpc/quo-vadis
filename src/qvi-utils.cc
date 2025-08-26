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
    int base
) {
    try {
        return std::stoi(str, nullptr, base);
    }
    catch (const std::invalid_argument &e) { }
    catch (const std::out_of_range &e) { }
    throw qvi_runtime_error(QV_ERR_INVLD_ARG);
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
qvi_port_from_env(void)
{
    const cstr_t ports = getenv(QVI_ENV_PORT.c_str());
    if (!ports) {
        return QVI_PORT_UNSET;
    }
    return qvi_stoi(std::string(ports));
}

int64_t
qvi_cantor_pairing(
    int a,
    int b
) {
    return (a + b) * ((a + b + 1) / 2) + b;
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

    int rc = QV_SUCCESS;

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
        rc = discover_fn(portno);
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
    return rc;
}

static int
get_portno_from_pid_cmdline(
    pid_t pid
) {
    std::vector<std::string> argv;
    int rc = qvi_pid_cmdline(pid, argv);
    if (qvi_unlikely(rc != QV_SUCCESS)) return QVI_PORT_UNSET;

    const size_t len = argv.size();
    for (size_t i = 0; i < len; ++i) {
        // This must match the command line
        // argument structure set by quo-vadisd.
        if (argv[i] != "--port") continue;
        return qvi_stoi(argv.at(i + 1));
    }
    return QVI_PORT_UNSET;
}

static int
get_portno_from_pid_environ(
    pid_t pid
) {
    const std::string delim = "=";
    std::vector<std::string> envs;
    int rc = qvi_pid_environ(pid, envs);
    if (qvi_unlikely(rc != QV_SUCCESS)) return QVI_PORT_UNSET;

    const size_t len = envs.size();
    for (size_t i = 0; i < len; ++i) {
        if (envs[i].rfind(QVI_ENV_PORT + delim, 0) == 0) {
            const std::string kval = envs[i];
            const size_t pos = kval.find(delim);
            return qvi_stoi(kval.substr(pos + delim.length()));
        }
    }
    return QVI_PORT_UNSET;
}

static int
discover_impl(
    int &target_port
) {
    std::vector<pid_t> pids;
    int rc = qvi_running(QVI_DAEMON_NAME, pids);
    if (qvi_unlikely(rc != QV_SUCCESS)) return rc;

    for (const auto &pid : pids) {
        int port = get_portno_from_pid_cmdline(pid);
        if (port == QVI_PORT_UNSET) {
            port = get_portno_from_pid_environ(pid);
            if (port == QVI_PORT_UNSET) continue;
        }
        // The caller doesn't care which port to use.
        if (target_port == QVI_PORT_UNSET) {
            target_port = port;
            return QV_SUCCESS;
        }
        // Found a daemon that is using the requested port.
        else if (target_port == port) return QV_SUCCESS;
    }
    return QV_ERR_NOT_FOUND;
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
qvi_start_quo_vadisd(
    int portno
) {
    const pid_t pid = fork();
    // Fork failed.
    if (qvi_unlikely(pid == -1)) {
        qvi_log_error(
            "\n\n#############################################\n"
            "# fork() failed while starting {}"
            "\n#############################################\n\n",
            QVI_DAEMON_NAME
        );
        return QV_ERR_SYS;
    }
    // Child
    else if (pid == 0) {
        std::vector<std::string> argss = {
            QVI_DAEMON_NAME,
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
            "# Could not start {}.\n#\n"
            "# For automatic startup, ensure the path to\n"
            "# {} is in your PATH and try again.\n"
            "# For manual startup, ensure {} is\n"
            "# running across all the servers in your job."
            "\n#############################################\n\n",
            QVI_DAEMON_NAME, QVI_DAEMON_NAME, QVI_DAEMON_NAME
        );
        _exit(EXIT_FAILURE);
    }
    return QV_SUCCESS;
}

static inline bool
is_numeric(
    const std::string &str
) {
    return !str.empty() && std::all_of(str.begin(), str.end(), ::isdigit);
}

static inline bool
is_pid_directory(
    const std::filesystem::path &p
) {
    return is_numeric(p.filename().string());
}

int
qvi_running(
    const std::string &name,
    std::vector<pid_t> &pids
) {
    namespace fs = std::filesystem;
    int rc = QV_SUCCESS;

    try {
        for (const auto &entry : fs::directory_iterator("/proc")) {
            if (!entry.is_directory()) continue;
            if (!is_pid_directory(entry.path())) continue;
            fs::path comm_path = entry.path() / "comm";
            if (!fs::exists(comm_path)) continue;
            std::ifstream comm_file(comm_path);
            std::string process_name;
            if (std::getline(comm_file, process_name)) {
                // Remove potential newline character.
                if (!process_name.empty() &&
                    process_name.back() == '\n') {
                    process_name.pop_back();
                }
                if (process_name != name) continue;
                const pid_t pid = qvi_stoi(entry.path().filename().string());
                if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
                pids.push_back(pid);
            }
        }
    }
    catch (const fs::filesystem_error &e) {
        qvi_log_error(e.what());
        rc = QV_ERR_FILE_IO;
    }
    return rc;
}

int
qvi_pid_cmdline(
    pid_t pid,
    std::vector<std::string> &argv
) {
    namespace fs = std::filesystem;
    int rc = QV_SUCCESS;

    try {
        fs::path cmdline("/proc/" + std::to_string(pid) + "/cmdline");
        if (!fs::exists(cmdline)) return QV_ERR_NOT_FOUND;
        // Open as binary to ensure every byte is read exactly as it is.
        std::ifstream file(cmdline.string(), std::ios::binary);
        // Read the entire content of the file into a string
        std::string content(
            (std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>()
        );
        // Tokenize the string by null bytes.
        std::string current_arg;
        for (char c : content) {
            if (c == '\0') {
                // Avoid adding empty strings if multiple nulls occur.
                if (!current_arg.empty()) {
                    argv.push_back(current_arg);
                    current_arg.clear();
                }
            }
            else {
                current_arg += c;
            }
        }
    }
    catch (const fs::filesystem_error &e) {
        qvi_log_error(e.what());
        rc = QV_ERR_FILE_IO;
    }
    return rc;
}

int
qvi_pid_environ(
    pid_t pid,
    std::vector<std::string> &envs
) {
    namespace fs = std::filesystem;
    int rc = QV_SUCCESS;

    try {
        fs::path environ("/proc/" + std::to_string(pid) + "/environ");
        if (!fs::exists(environ)) return QV_ERR_NOT_FOUND;
        // Open as binary to ensure every byte is read exactly as it is.
        std::ifstream file(environ.string(), std::ios::binary);
        // Read the entire content of the file into a string
        std::string content(
            (std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>()
        );
        // Split the content by the null byte character.
        std::size_t start = 0;
        std::size_t end = content.find('\0');

        while (end != std::string::npos) {
            envs.push_back(content.substr(start, end - start));
            start = end + 1;
            end = content.find('\0', start);
        }
        // Handle the final entry if there is no trailing null byte.
        if (start < content.length()) {
            envs.push_back(content.substr(start));
        }
    }
    catch (const fs::filesystem_error &e) {
        qvi_log_error(e.what());
        rc = QV_ERR_FILE_IO;
    }
    return rc;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
