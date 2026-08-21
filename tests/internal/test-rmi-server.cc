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
 * @file test-rmi-server.cc
 *
 * Fault-injection tests for the RMI server in qvi-rmi.cc.
 *
 * Rather than going through the well-behaved qvi_rmi_client, these tests talk
 * to the server directly over a raw ZeroMQ REQ socket and send deliberately
 * malformed, truncated, empty, or otherwise hostile messages. The goal is to
 * probe the server's error-handling paths and answer two questions for each
 * case:
 *
 *   1. Does the server survive (no crash / no abort)?
 *   2. Does the server stay responsive afterward (no hang, no wedged
 *      REQ/REP state machine)?
 *
 * Because a hostile message may crash or hang the server, each fault case runs
 * against its own freshly forked server child. The parent is the test harness:
 * it crafts the raw bytes, sends them, applies receive timeouts to detect
 * hangs, and reaps the child to distinguish a clean exit from a crash. A watch-
 * dog alarm bounds the total runtime so a wedged server can never hang CI.
 *
 * The RMI wire format (see qvi-rmi.cc) is:
 *
 *   [ qvi_rmi_msg_header ][ size_t payload_len ][ cereal binary payload ]
 *
 * where qvi_rmi_msg_header currently holds a single qvi_rmi_rpc_fid_t (fid).
 */

#include "quo-vadis.h"
#include "qvi-utils.h"
#include "qvi-hwloc.h"
#include "qvi-rmi.h"
#include "zmq.h"

#include "common-test-utils.h"

#include <csignal>
#include <sys/wait.h>

namespace {

// Receive timeout (ms) for the harness REQ socket. If the server fails to
// reply within this window we treat the case as a hang/failure.
constexpr int RECV_TIMEOUT_MS = 4000;
// Overall watchdog (seconds) covering a single fault case.
constexpr unsigned WATCHDOG_SECS = 30;

// The header layout must match qvi-rmi.cc's private qvi_rmi_msg_header. It is a
// single qvi_rmi_rpc_fid_t. We replicate it here to build raw messages.
struct wire_header {
    qvi_rmi_rpc_fid_t fid = QVI_RMI_FID_INVALID;
};

// ---------------------------------------------------------------------------
// Server child
// ---------------------------------------------------------------------------

// Runs a real qvi_rmi_server bound to url. Intended to be executed in a forked
// child; it blocks in the service loop until it receives a shutdown message or
// is killed by the parent.
[[noreturn]] static void
run_server_child(
    const std::string &url
) {
    qvi_hwloc hwloc;
    qvi_rmi_config config;
    qvi_rmi_server server;

    int rc = hwloc.topology_init(QVI_HWLOC_FLAG_TOPO_FULL);
    if (rc != QV_SUCCESS) _exit(101);

    rc = hwloc.topology_load();
    if (rc != QV_SUCCESS) _exit(102);

    rc = hwloc.topology_export(qvi_tmpdir());
    if (rc != QV_SUCCESS) _exit(103);

    config.url = url;
    rc = server.configure(config);
    if (rc != QV_SUCCESS) _exit(104);

    // Blocks until shutdown. Returns QV_SUCCESS on a clean shutdown.
    rc = server.start();
    _exit(rc == QV_SUCCESS ? 0 : 105);
}

// ---------------------------------------------------------------------------
// Raw ZMQ REQ helpers (harness side)
// ---------------------------------------------------------------------------

struct raw_client {
    void *ctx = nullptr;
    void *sock = nullptr;

    explicit raw_client(const std::string &url)
    {
        ctx = zmq_ctx_new();
        ctu_assert(ctx != nullptr, "zmq_ctx_new() failed");
        sock = zmq_socket(ctx, ZMQ_REQ);
        ctu_assert(sock != nullptr, "zmq_socket(ZMQ_REQ) failed");

        const int timeout = RECV_TIMEOUT_MS;
        int zrc = zmq_setsockopt(sock, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
        ctu_assert(zrc == 0, "zmq_setsockopt(ZMQ_RCVTIMEO) failed");
        // Bound send blocking too so a dead peer cannot wedge us.
        zrc = zmq_setsockopt(sock, ZMQ_SNDTIMEO, &timeout, sizeof(timeout));
        ctu_assert(zrc == 0, "zmq_setsockopt(ZMQ_SNDTIMEO) failed");
        // Do not linger on close; we never want to hang on teardown.
        const int linger = 0;
        zrc = zmq_setsockopt(sock, ZMQ_LINGER, &linger, sizeof(linger));
        ctu_assert(zrc == 0, "zmq_setsockopt(ZMQ_LINGER) failed");

        zrc = zmq_connect(sock, url.c_str());
        ctu_assert(zrc == 0, "zmq_connect(%s) failed", url.c_str());
    }

    ~raw_client(void)
    {
        if (sock) zmq_close(sock);
        if (ctx) zmq_ctx_destroy(ctx);
    }

    // Sends raw bytes. Returns the zmq_send() result.
    int
    send_raw(const void *data, size_t len)
    {
        return zmq_send(sock, data, len, 0);
    }

    // Attempts to receive a reply. Returns true if a reply arrived within the
    // timeout, false on EAGAIN (timeout / no reply).
    bool
    recv_reply(void)
    {
        zmq_msg_t msg;
        int zrc = zmq_msg_init(&msg);
        ctu_assert(zrc == 0, "zmq_msg_init() failed");
        const int n = zmq_msg_recv(&msg, sock, 0);
        zmq_msg_close(&msg);
        return n != -1;
    }
};

// Builds a raw message: header (fid) followed by an arbitrary payload blob.
static std::vector<byte_t>
build_message(
    qvi_rmi_rpc_fid_t fid,
    const void *payload,
    size_t payload_len
) {
    wire_header hdr;
    hdr.fid = fid;

    std::vector<byte_t> buf(sizeof(hdr) + payload_len);
    memcpy(buf.data(), &hdr, sizeof(hdr));
    if (payload_len) {
        memcpy(buf.data() + sizeof(hdr), payload, payload_len);
    }
    return buf;
}

// Like build_message(), but writes an arbitrary raw integer function ID into
// the header bytes. Used to inject a wire fid that is not a valid enumerator of
// qvi_rmi_rpc_fid_t (a hostile client is not obliged to respect the enum).
// Writing the bytes directly avoids materializing an out-of-range enum value,
// which would be undefined behavior.
static std::vector<byte_t>
build_message_raw_fid(
    int raw_fid,
    const void *payload,
    size_t payload_len
) {
    // The header currently holds a single fid field; mirror its layout. The
    // raw integer we write must match the size of that field exactly.
    static_assert(
        sizeof(wire_header) == sizeof(qvi_rmi_rpc_fid_t),
        "unexpected wire_header layout"
    );
    static_assert(
        sizeof(raw_fid) == sizeof(qvi_rmi_rpc_fid_t),
        "raw fid type does not match the on-wire fid size"
    );
    std::vector<byte_t> buf(sizeof(wire_header) + payload_len);
    memcpy(buf.data(), &raw_fid, sizeof(raw_fid));
    if (payload_len) {
        memcpy(buf.data() + sizeof(wire_header), payload, payload_len);
    }
    return buf;
}

// ---------------------------------------------------------------------------
// Server lifecycle management for a single fault case
// ---------------------------------------------------------------------------

// Forks a server child bound to url and returns its PID. Waits briefly to give
// the child time to bind before returning.
static pid_t
spawn_server(
    const std::string &url
) {
    const pid_t pid = fork();
    ctu_assert(pid >= 0, "fork() failed");
    if (pid == 0) {
        run_server_child(url);
        // Not reached.
    }
    // Give the child a moment to bind its socket.
    usleep(300 * 1000);
    return pid;
}

// Result of reaping a server child.
enum class reap_status {
    CLEAN_EXIT,   // exited 0
    NONZERO_EXIT, // exited non-zero
    CRASHED,      // killed by a signal (e.g., SIGABRT from qvi_abort())
    STILL_RUNNING // did not exit within the grace window
};

// Waits up to grace_ms for the child to exit on its own. Returns how it went.
static reap_status
try_reap(
    pid_t pid,
    int *exit_code_out,
    int *signal_out,
    int grace_ms
) {
    if (exit_code_out) *exit_code_out = -1;
    if (signal_out) *signal_out = 0;

    const int step_ms = 50;
    for (int waited = 0; waited <= grace_ms; waited += step_ms) {
        int status = 0;
        const pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid) {
            if (WIFSIGNALED(status)) {
                if (signal_out) *signal_out = WTERMSIG(status);
                return reap_status::CRASHED;
            }
            if (WIFEXITED(status)) {
                const int code = WEXITSTATUS(status);
                if (exit_code_out) *exit_code_out = code;
                return code == 0
                    ? reap_status::CLEAN_EXIT
                    : reap_status::NONZERO_EXIT;
            }
        }
        usleep(step_ms * 1000);
    }
    return reap_status::STILL_RUNNING;
}

// Force-kills and reaps a child that is still running.
static void
kill_and_reap(
    pid_t pid
) {
    kill(pid, SIGKILL);
    int status = 0;
    (void)waitpid(pid, &status, 0);
}

// Sends a well-formed shutdown request over raw so a surviving server exits
// cleanly. Returns whether the send itself succeeded.
//
// Note: the server sets ZMQ_LINGER=0 on its socket and closes it immediately
// after handling a shutdown, so the shutdown *reply* is intentionally racy and
// may be discarded before transmission (see qvi_rmi_server::start()). We
// therefore do NOT require a reply here; the proof that the server is still
// responsive comes from the fault message's reply, and the proof that shutdown
// worked comes from the server process exiting cleanly.
static bool
send_shutdown(
    raw_client &cli
) {
    const auto msg = build_message(QVI_RMI_FID_SERVER_SHUTDOWN, nullptr, 0);
    const int n = cli.send_raw(msg.data(), msg.size());
    if (n == -1) return false;
    // Best-effort drain of a possible reply; ignore the result.
    (void)cli.recv_reply();
    return true;
}

// ---------------------------------------------------------------------------
// Fault cases
// ---------------------------------------------------------------------------

// Verifies the server did not die from processing a fault, then, over the SAME
// REQ socket, performs a clean shutdown and confirms it exits cleanly. A single
// socket is used throughout so we honor strict ZMQ REQ/REP send/recv
// alternation and avoid cross-peer REP routing ambiguity.
static void
expect_survives_and_shuts_down(
    const char *case_name,
    raw_client &cli,
    pid_t server_pid
) {
    // The server must NOT have died from processing the fault message.
    int exit_code = 0, sig = 0;
    reap_status st = try_reap(server_pid, &exit_code, &sig, 0);
    if (st == reap_status::CRASHED) {
        kill_and_reap(server_pid);
        ctu_panic(
            "[%s] server CRASHED (signal %d) while handling fault",
            case_name, sig
        );
    }
    if (st == reap_status::NONZERO_EXIT) {
        kill_and_reap(server_pid);
        ctu_panic(
            "[%s] server exited non-zero (%d) while handling fault",
            case_name, exit_code
        );
    }

    // Send a well-formed shutdown request over the same socket. The server's
    // responsiveness after the fault has already been proven by its reply to
    // the fault message (see run_single_message_case); here we drive it to
    // exit. A wedged/hung server manifests as a failure to exit cleanly below.
    const bool sent = send_shutdown(cli);
    if (!sent) {
        kill_and_reap(server_pid);
        ctu_panic(
            "[%s] could not send shutdown request after the fault", case_name
        );
    }

    // It should now exit cleanly. If it does not, the server is hung/wedged.
    st = try_reap(server_pid, &exit_code, &sig, 5000);
    if (st == reap_status::CRASHED) {
        kill_and_reap(server_pid);
        ctu_panic(
            "[%s] server CRASHED (signal %d) during shutdown", case_name, sig
        );
    }
    if (st != reap_status::CLEAN_EXIT) {
        kill_and_reap(server_pid);
        ctu_panic(
            "[%s] server failed to shut down cleanly after the fault "
            "(status=%d; likely hung / wedged)",
            case_name, (int)st
        );
    }
    printf("# ✓ [%s] server survived and shut down cleanly\n", case_name);
}

// Sends a single raw message to a fresh server over a persistent REQ socket,
// reads the reply (the server is expected to always answer), then reuses that
// same socket to verify survival + responsiveness via a clean shutdown.
static void
run_single_message_case(
    const char *case_name,
    const std::string &url,
    const std::vector<byte_t> &msg
) {
    const pid_t server_pid = spawn_server(url);

    raw_client cli(url);
    const int n = cli.send_raw(msg.data(), msg.size());
    // A send failure is itself acceptable; we only care that the server does
    // not crash/hang. If the send succeeded, drain the reply so the REQ socket
    // is ready for the subsequent shutdown request on the same socket.
    if (n != -1) {
        const bool replied = cli.recv_reply();
        if (!replied) {
            kill_and_reap(server_pid);
            ctu_panic(
                "[%s] server did not reply to the fault message "
                "(likely hung / wedged REP socket)",
                case_name
            );
        }
    }
    else {
        // The message could not even be sent; fall back to a fresh socket so
        // the REQ state machine is not left mid-transaction.
        raw_client fresh(url);
        expect_survives_and_shuts_down(case_name, fresh, server_pid);
        return;
    }
    expect_survives_and_shuts_down(case_name, cli, server_pid);
}

// Case: empty (zero-byte) message.
static void
case_empty_message(
    const std::string &url
) {
    std::vector<byte_t> msg; // zero bytes
    run_single_message_case("empty-message", url, msg);
}

// Case: message shorter than the header. Exercises unpack_msg_header()'s
// unconditional memmove of sizeof(header) bytes from a short buffer.
static void
case_truncated_header(
    const std::string &url
) {
    std::vector<byte_t> msg(sizeof(wire_header) - 1, byte_t{0xAB});
    run_single_message_case("truncated-header", url, msg);
}

// Case: valid header for a real RPC, but no payload at all. The RPC handler
// will attempt qvi_bbuff::unpack() on a zero-length body.
static void
case_valid_fid_no_payload(
    const std::string &url
) {
    const auto msg = build_message(QVI_RMI_FID_GET_CPUBIND, nullptr, 0);
    run_single_message_case("valid-fid-no-payload", url, msg);
}

// Case: valid header, but a bogus cereal length prefix claiming a huge payload
// that is not actually present. Probes qvi_bbuff::unpack()'s handling of a
// length that exceeds the real buffer (potential huge alloc / OOB read).
static void
case_bogus_length_prefix(
    const std::string &url
) {
    const size_t bogus_len = (size_t)1 << 40; // ~1 TiB claimed
    const auto msg = build_message(
        QVI_RMI_FID_GET_CPUBIND, &bogus_len, sizeof(bogus_len)
    );
    run_single_message_case("bogus-length-prefix", url, msg);
}

// Case: valid header + valid length prefix, but the payload bytes are garbage
// that cereal cannot deserialize into the expected argument types.
static void
case_garbage_payload(
    const std::string &url
) {
    // Claim a small payload and provide that many random-ish bytes.
    const size_t claimed = 16;
    std::vector<byte_t> payload(sizeof(size_t) + claimed);
    memcpy(payload.data(), &claimed, sizeof(size_t));
    for (size_t i = 0; i < claimed; ++i) {
        payload[sizeof(size_t) + i] = (byte_t)(0xDE ^ (i * 7));
    }
    const auto msg = build_message(
        QVI_RMI_FID_GET_INTRINSIC_HWPOOL, payload.data(), payload.size()
    );
    run_single_message_case("garbage-payload", url, msg);
}

// Case: an out-of-range / unknown function ID. m_rpc_dispatch() should reject
// it. The concern is that on the "unknown fid" path the server breaks out of
// dispatch WITHOUT sending a reply, which wedges the ZMQ REP socket so the
// next REQ client hangs forever.
static void
case_unknown_fid(
    const std::string &url
) {
    // Well past the last defined enumerator. Injected as a raw integer so we
    // never form an out-of-range enum value (which would be UB).
    const int bogus_fid = QVI_RMI_FID_GET_INTRINSIC_HWPOOL + 1000;
    const auto msg = build_message_raw_fid(bogus_fid, nullptr, 0);
    run_single_message_case("unknown-fid", url, msg);
}

// Case: the reserved QVI_RMI_FID_INVALID (0). It maps to s_rpc_invalid() which
// calls qvi_abort(). This documents/detects that a client can crash the server
// by sending fid 0.
static void
case_invalid_fid(
    const std::string &url
) {
    const auto msg = build_message(QVI_RMI_FID_INVALID, nullptr, 0);
    run_single_message_case("invalid-fid", url, msg);
}

// Case: sanity control. A completely well-formed shutdown message must be
// handled cleanly. If this fails, the harness (not the server) is suspect.
static void
case_control_clean_shutdown(
    const std::string &url
) {
    const pid_t server_pid = spawn_server(url);
    {
        raw_client cli(url);
        const bool sent = send_shutdown(cli);
        ctu_assert(sent, "[control] could not send clean shutdown");
    }
    int exit_code = 0, sig = 0;
    const reap_status st = try_reap(server_pid, &exit_code, &sig, 5000);
    if (st != reap_status::CLEAN_EXIT) {
        kill_and_reap(server_pid);
        ctu_panic("[control] clean shutdown did not exit cleanly (status=%d)",
            (int)st);
    }
    printf("# ✓ [control] clean shutdown handled\n");
}

} // namespace

static void
watchdog_handler(int)
{
    static const char msg[] = "\ntest-rmi-server: WATCHDOG TIMEOUT\n";
    ssize_t w = write(STDERR_FILENO, msg, sizeof(msg) - 1);
    (void)w;
    _exit(EXIT_FAILURE);
}

int
main(
    int argc,
    char **argv
) {
    setbuf(stdout, nullptr);

    if (argc != 2) {
        fprintf(stderr, "Usage: %s URL\n", argv[0]);
        return EXIT_FAILURE;
    }
    const std::string url = argv[1];

    // A dead child must not turn a blocking send into a SIGPIPE-style abort;
    // ZMQ handles EPIPE internally, but be defensive.
    signal(SIGPIPE, SIG_IGN);

    printf("# Starting RMI server fault-injection tests (%s)\n", url.c_str());

    // Per-case watchdog so a wedged server can never hang the suite.
    signal(SIGALRM, watchdog_handler);

    struct {
        const char *name;
        void (*fn)(const std::string &);
    } cases[] = {
        {"control-clean-shutdown", case_control_clean_shutdown},
        {"empty-message",          case_empty_message},
        {"truncated-header",       case_truncated_header},
        {"valid-fid-no-payload",   case_valid_fid_no_payload},
        {"bogus-length-prefix",    case_bogus_length_prefix},
        {"garbage-payload",        case_garbage_payload},
        {"unknown-fid",            case_unknown_fid},
        {"invalid-fid",            case_invalid_fid},
    };

    for (const auto &c : cases) {
        printf("# --- case: %s ---\n", c.name);
        alarm(WATCHDOG_SECS);
        c.fn(url);
        alarm(0);
    }

    printf("# ✓ All RMI server fault-injection tests PASSED\n");
    return EXIT_SUCCESS;
}

/*
 * vim: ft=cpp ts=4 sts=4 sw=4 expandtab
 */
