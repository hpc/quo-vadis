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
 * @file quo-vadisd.cc
 *
 * Implements the quo-vadis daemon.
 */

#include "qvi-utils.h"
#include "qvi-hwloc.h"
#include "qvi-rmi.h"

static const std::string app_name = "quo-vadisd";

using option_help = std::map<std::string, std::string>;

struct qvid {
    qvi_rmi_server rmi;
    qvi_rmi_config rmic;
    /** Base session directory. */
    std::string session_dir = {};
    /** Run as a daemon flag. */
    bool daemonized = true;
    /** Constructor. */
    qvid(void)
    {
        const int rc = qvi_new(&rmic.hwloc);
        if (qvi_unlikely(rc != QV_SUCCESS)) throw qvi_runtime_error();
    }
    /** Destructor. */
    ~qvid(void)
    {
        qvi_delete(&rmic.hwloc);
    }

    void
    closefds(void)
    {
        qvi_log_info("Closing FDs");
        // Determine the max number of file descriptors.
        struct rlimit rl;
        if (getrlimit(RLIMIT_NOFILE, &rl) < 0) {
            const int err = errno;
            const cstr_t ers = "Cannot determine RLIMIT_NOFILE";
            qvi_panic_log_error("{} (rc={}, {})", ers, err, strerror(err));
        }
        // Default: no limit on this resource, so pick one.
        int64_t maxfd = 1024;
        if (rl.rlim_max != RLIM_INFINITY) {
            // Not RLIM_INFINITY, so set to resource limit.
            maxfd = (int64_t)rl.rlim_max;
        }
        // Close all the file descriptors.
        for (int64_t fd = 0; fd < maxfd; ++fd) {
            (void)close(fd);
        }
    }

    void
    become_session_leader(void)
    {
        qvi_log_info("Becoming session leader");

        pid_t pid = 0;
        if ((pid = fork()) < 0) {
            const int err = errno;
            const cstr_t ers = "fork() failed";
            qvi_panic_log_error("{} (rc={}, {})", ers, err, strerror(err));
        }
        // Parent
        if (pid != 0) {
            // _exit(2) used to match daemon(3) behavior.
            _exit(EXIT_SUCCESS);
        }
        // Child
        const pid_t pgid = setsid();
        if (pgid < 0) {
            const int err = errno;
            const cstr_t ers = "setsid() failed";
            qvi_panic_log_error("{} (rc={}, {})", ers, err, strerror(err));
        }
    }

    void
    configure_rmi(void)
    {
        qvi_log_info("Configuring RMI");

        const int rc = rmi.configure(rmic);
        if (qvi_unlikely(rc != QV_SUCCESS)) {
            qvi_panic_log_error("rmi.configure() failed");
        }

        qvi_log_info("--URL: {}", rmic.url);
        qvi_log_info("--Port Number: {}", rmic.portno);
        qvi_log_info("--hwloc XML: {}", rmic.hwtopo_path);
    }

    void
    start_rmi_server(void)
    {
        qvi_log_info("Starting RMI");

        const int rc = rmi.start();
        if (qvi_unlikely(rc != QV_SUCCESS)) {
            const cstr_t ers = "qvi_rmi_server_start() failed";
            qvi_panic_log_error("{} (rc={}, {})", ers, rc, qv_strerr(rc));
        }
    }

    void
    load_hwtopo(void)
    {
        qvi_log_info("Loading hardware information");

        int rc = rmic.hwloc->topology_init(nullptr);
        if (qvi_unlikely(rc != QV_SUCCESS)) {
            static cstr_t ers = "qvi_hwloc_topology_init() failed";
            qvi_panic_log_error("{} (rc={}, {})", ers, rc, qv_strerr(rc));
        }

        rc = rmic.hwloc->topology_load();
        if (qvi_unlikely(rc != QV_SUCCESS)) {
            static cstr_t ers = "qvi_hwloc_topology_load() failed";
            qvi_panic_log_error("{} (rc={}, {})", ers, rc, qv_strerr(rc));
        }
    }

    void
    determine_connection_info(void)
    {
        qvi_log_info("Determining connection information");

        const int rc = qvi_rmi_get_url(rmic.url, rmic.portno);
        if (qvi_unlikely(rc != QV_SUCCESS)) {
            qvi_panic_log_error(qvi_rmi_conn_env_ers());
        }
    }

    void
    make_session_dir(void)
    {
        qvi_log_info("Creating session directory");

        const std::string tmpdir = qvi_tmpdir();
        // Make sure that the provided temp dir is usable.
        int eno = 0;
        bool usable = qvi_access(tmpdir, R_OK | W_OK, &eno);
        if (qvi_unlikely(!usable)) {
            qvi_panic_log_error(
                "{} is not usable as a tmp dir (rc={}, {})",
                tmpdir, eno, strerror(eno)
            );
        }
        // Make sure that this session directory does not already exist. If it
        // does, then we can't continue because another daemon is using it.
        const std::string portnos = std::to_string(rmic.portno);
        const std::string session_dirname = app_name + "." + portnos;
        const std::string full_session_dir = tmpdir + "/" + session_dirname;
        usable = qvi_access(full_session_dir, R_OK, &eno);
        if (qvi_unlikely(usable)) {
            const std::string errs =
                "A {} session directory was detected at {}. "
                "This is typically caused by the following situations: "
                "1. {} is already running using port {}. "
                "Clients may use this existing connection. "
                "If a new connection is desired, choose a different port "
                "and try again. 2. A daemon was forcibly killed and did "
                "not cleanup completely. In this case, remove {} "
                "and try again.";
            qvi_panic_log_error(
                errs, app_name, full_session_dir, app_name,
                rmic.portno, full_session_dir
            );
        }
        const int rc = mkdir(full_session_dir.c_str(), 0755);
        if (qvi_unlikely(rc != 0)) {
            eno = errno;
            qvi_panic_log_error(
                "Failed to create session dir {} (rc={}, {})",
                full_session_dir , eno, strerror(eno)
            );
        }
        session_dir = full_session_dir;
    }

    void
    export_hwtopo(void)
    {
        qvi_log_info("Publishing hardware information");

        char *path = nullptr;
        const int rc = rmic.hwloc->topology_export(
            session_dir.c_str(), &path
        );
        if (qvi_unlikely(rc != QV_SUCCESS)) {
            const cstr_t ers = "qvi_hwloc_topology_export() failed";
            qvi_panic_log_error("{} (rc={}, {})", ers, rc, qv_strerr(rc));
        }
        rmic.hwtopo_path = std::string(path);
        free(path);
    }

    void
    cleanup(void)
    {
        qvi_log_info("Cleaning up");

        const int rc = qvi_rmall(session_dir);
        if (qvi_unlikely(rc != QV_SUCCESS)) {
            qvi_log_warn("Removal of {} failed.", session_dir);
        }
    }
};

static void
show_usage(
    const option_help &opt_help
) {
    qvi_log_info(
        "\nUsage:\n"
        "{} [OPTIONS]\n"
        "Options:"
        , app_name
    );

    for (auto &i : opt_help) {
        qvi_log_info("  {} {}", i.first, i.second);
    }
}

static int
parse_args(
    int argc,
    char **argv,
    qvid &qvd
) {
    enum {
        FLOOR = 256,
        HELP,
        NO_DAEMONIZE,
        PORT
    };

    const cstr_t opts = "";
    const struct option lopts[] = {
        {"help"            , no_argument,       nullptr, HELP                 },
        {"no-daemonize"    , no_argument,       nullptr, NO_DAEMONIZE         },
        {"port"            , required_argument, nullptr, PORT                 },
        {nullptr           , 0,                 nullptr, 0                    }
    };
    static const option_help opt_help = {
        {"[--help]             ", "Show this message and exit."               },
        {"[--no-daemonize]     ", "Do not run as a daemon."                   },
        {"[--port PORTNO]      ", "Specify port number to use."               }
    };

    int opt;
    while (-1 != (opt = getopt_long_only(argc, argv, opts, lopts, nullptr))) {
        switch (opt) {
            case HELP:
                show_usage(opt_help);
                return QV_SUCCESS_SHUTDOWN;
            case NO_DAEMONIZE:
                qvd.daemonized = false;
                break;
            case PORT: {
                const int rc = qvi_stoi(std::string(optarg), qvd.rmic.portno);
                if (qvi_unlikely(rc != QV_SUCCESS)) return rc;
                break;
            }
            default:
                show_usage(opt_help);
                return QV_ERR_INVLD_ARG;
        }
    }
    // Make sure no bogus options were provided.
    if (optind < argc) {
        qvi_log_warn(
            "{}: Unrecognized option detected: \'{}\'",
            app_name, argv[optind]
        );
        show_usage(opt_help);
        return QV_ERR_INVLD_ARG;
    }
    return QV_SUCCESS;
}

static int
start(
    int argc,
    char **argv
) {
    try {
        qvid qvd;

        int rc = parse_args(argc, argv, qvd);
        if (rc != QV_SUCCESS) {
            if (rc == QV_SUCCESS_SHUTDOWN) {
                rc = QV_SUCCESS;
            }
            return rc;
        }

        if (qvd.daemonized) {
            // Redirect all console output to syslog.
            qvi_logger::console_to_syslog();
            // Clear umask. Note: this system call always succeeds.
            umask(0);
            // Become a session leader to lose controlling TTY.
            qvd.become_session_leader();
            // Close all file descriptors.
            qvd.closefds();
        }
        // Determine connection information.
        qvd.determine_connection_info();
        // Create our session directory.
        qvd.make_session_dir();
        // Gather and publish hardware information.
        qvd.load_hwtopo();
        qvd.export_hwtopo();
        // Configure RMI, start listening for commands.
        qvd.configure_rmi();
        // This blocks until it is instructed to shutdown.
        qvd.start_rmi_server();
        // Cleanup
        qvd.cleanup();
        return QV_SUCCESS;
    }
    qvi_catch_and_return();
}

int
main(
    int argc,
    char **argv
) {
    const int rc = start(argc, argv);
    return (rc == QV_SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE);
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
