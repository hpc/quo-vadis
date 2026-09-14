# MPI Message-Rate Benchmark

`qvb-mpi-msg-rate` measures the message rate of the Quo-Vadis coordination
daemon, `quo-vadisd`, as experienced through the public Quo-Vadis API. It
reports the effective rate a real user should expect (including all client-side
library overhead) rather than the raw rate of the internal transport, and
repeats trials so the numbers come with error bars.

## Overview

`quo-vadisd` handles client requests through a ZeroMQ `REP` socket, one request
at a time: it receives a request, processes it, and sends exactly one reply
before accepting the next. This gives the daemon a finite service rate. As the
number of concurrent clients grows, aggregate throughput rises until the daemon
saturates and then plateaus; that plateau is the maximum message rate.

This benchmark drives the daemon through the public API and sweeps the number of
concurrent clients to locate that plateau, reporting both throughput
(messages/second) and latency (per-message round-trip time, including
percentiles).

## Methodology

The benchmark follows the standard closed-loop server-benchmarking method.

- **Concurrency = MPI processes.** Each MPI process is an independent client. A
  process obtains its own scope with `qv_mpi_scope()`, which establishes its own
  connection to `quo-vadisd`. The number of concurrent clients `N` is therefore
  the MPI communicator size, chosen with `mpiexec -n N`. Concurrency is
  launcher-driven: to sweep `N`, re-run the binary at several process counts.

- **Driven primitive: `qv_bind_push()` / `qv_bind_pop()`.** On the daemon path
  each of these calls is exactly one synchronous RMI round-trip (`SET_CPUBIND`).
  The pair is issued together every iteration to keep the bind stack balanced,
  so each timed iteration generates two daemon messages.

  > **Why not other calls?** Many public functions---`qv_hw_obj_count`,
  > `qv_group_size`, `qv_bind_string`, `qv_device_id`, `qv_barrier`---are served
  > entirely from the client-local `hwloc` cache populated at connect time. They
  > perform no daemon round-trips and are unsuitable for measuring the daemon
  > message rate.

- **Closed loop with warmup and a bounded window.** Each process connects once,
  runs an untimed warmup to amortize first-call and cache effects, then all
  processes synchronize on a `MPI_Barrier()` before opening a fixed measurement
  window. The window is either a wall-clock duration (`--duration`) or a fixed
  request count per process (`--requests`). A closing `MPI_Barrier()` bounds the
  window so per-process windows overlap and true concurrency is measured.

- **Aggregation.** Per-message round-trip latencies are recorded into a
  bounded, log-spaced histogram. At the end of each trial the per-process
  results are reduced across all processes with MPI collectives; only the root
  process prints. The aggregate throughput is

  ```
  throughput = sum(messages over all processes) / max(per-process window)
  ```

- **Trials and error bars.** With `--trials k` the whole measurement is repeated
  `k` times and the root process reports the throughput mean, sample standard
  deviation, and a 95% confidence interval (normal approximation).

- **Sanity check (Little's Law).** For a closed loop, the number of in-flight
  requests should satisfy `L = throughput × mean_latency`, and with one message
  in flight per client that estimate should track the client count `N`. After
  the trials, the root process prints `L` and the ratio `L/N`. If `L/N` falls
  outside the range `0.70`–`1.10` it prints a warning and the program exits with
  a non-zero status. Low client counts (especially `N = 1`) can trip this
  because per-iteration and barrier overhead inflates the window without being
  counted as latency.

### Process binding matters

The MPI process binding policy is a first-order factor in the results and must
be chosen deliberately and recorded. It materially changes both throughput and
latency:

- **Bound.** Pinning each client to a dedicated core keeps the OS scheduler from
  migrating clients between cores, avoids cross-core cache and run-queue
  contention, and produces stable, reproducible numbers. For example:

  ```shell
  mpiexec -n 4 --bind-to core ./tests/benchmarks/qvb-mpi-msg-rate \
      --duration 5 --trials 10
  ```

- **Loose / unbound.** With a less restrictive policy the OS is free to migrate
  clients and to co-schedule several of them on the same core, which
  changes---and typically lowers and destabilizes---the measured maximum message
  rate:

  ```shell
  mpiexec -n 4 --bind-to none ./tests/benchmarks/qvb-mpi-msg-rate \
      --duration 5 --trials 10
  ```

The same benchmark invocation can therefore report different results under
different binding policies, so a run under `--bind-to core` is not directly
comparable to one with looser binding.

Binding controls vary by MPI implementation (e.g. `--bind-to core|hwthread|none`
and `--map-by ...` in Open MPI and MPICH/Hydra; some launchers also expose
`OMPI_*` or `HYDRA_*` environment variables). The exact launcher command line is
not captured in the benchmark banner (it is not visible to the launched
process), so record the full `mpiexec` command (including the binding flags)
alongside your results.

## Building

The benchmark builds with the rest of the project and requires MPI (it is only
built when `MPI_FOUND`). Using the existing configured build directory:

```shell
cmake --build build -j
```

The resulting binary is `build/tests/benchmarks/qvb-mpi-msg-rate`.

## Running

### Prerequisites

A `quo-vadisd` must be reachable. The process/MPI client discovers the daemon
via the `QV_PORT` environment variable. Start a daemon and point clients at it:

```shell
# Start the daemon in the foreground on a chosen port.
quo-vadisd --no-daemonize --port 55999 &

# Tell clients which port to use.
export QV_PORT=55999
```

### A single concurrency point

```shell
mpiexec -n 8 ./build/tests/benchmarks/qvb-mpi-msg-rate \
    --duration 5 --warmup 1 --trials 10 --csv msgrate.csv
```

This runs eight concurrent clients, each looping `qv_bind_push`/`qv_bind_pop`
for a 5-second window, repeated for 10 trials, appending results to
`msgrate.csv`.

> **Choose a process binding policy.** The launcher's binding policy strongly
> affects the results. Add an explicit binding flag (e.g. `--bind-to core`) and
> record the full command; see [Process binding matters](#process-binding-matters).

### Sweeping the client count

Concurrency is controlled entirely by the launcher, so a sweep is a loop over
process counts. Appending to a single CSV builds the full
throughput-vs-concurrency curve:

```shell
export QV_PORT=55999
rm -f msgrate.csv
for n in 1 2 4 8 16 32 64; do
    mpiexec -n "$n" ./build/tests/benchmarks/qvb-mpi-msg-rate \
        --duration 5 --warmup 1 --trials 10 --csv msgrate.csv
done
```

### Command-line options

| Option           | Default | Description                                                              |
|------------------|---------|--------------------------------------------------------------------------|
| `--duration SEC` | `5.0`   | Timed window length per trial (seconds).                                 |
| `--requests N`   | `0`     | Fixed push/pop iterations per process per trial. Overrides `--duration`. |
| `--warmup SEC`   | `1.0`   | Untimed warmup before each trial (seconds).                              |
| `--trials K`     | `5`     | Number of measurement repetitions.                                       |
| `--csv PATH`     | (none)  | Append per-trial results as CSV (written by the root process).           |
| `-h`, `--help`   |         | Print usage and exit.                                                    |

The number of concurrent clients is not a flag; it is the MPI communicator size
set with `mpiexec -n N`.

## Output

Rank 0 (the root process) prints a reproducibility banner followed by one row
per trial and a summary line. For example:

```
# quo-vadisd message-rate benchmark (public API, MPI clients)
# host              : compute-node-01
# online processors : 128
# concurrent clients: 8 (MPI processes)
# quo-vadis version : 1.0.0
# mpi library       : Open MPI v5.0.0, ...
# QV_PORT           : 55999
# message primitive : push/pop (1 daemon message each)
# window            : 5.000 s
# warmup            : 1.000 s
# trials            : 10
#
# trial            msg/s       mean(ns)        p50(ns)        p95(ns)        p99(ns)        max(ns)
  0             412345.6         19100.0        16000.0        45000.0        90000.0        512000
  ...
#
# summary: clients=8 throughput_mean=411982.3 msg/s stddev=1203.5 ci95=+/-746.1 (n=10 trials)
# little's law: L=7.86 (throughput_mean * mean_latency) N=8 L/N=0.982
```

If `L/N` falls outside the accepted range, an extra line is printed and the
program exits non-zero:

```
# Warning: Little's Law check failed: L/N=0.412 outside [0.70, 1.10]
```

### CSV schema

When `--csv` is given, the root process appends one row per trial (writing a
header only when the file is empty):

| Column                  | Meaning                                             |
|-------------------------|-----------------------------------------------------|
| `clients`               | Number of concurrent clients (MPI communicator size)|
| `trial`                 | Zero-based trial index                              |
| `throughput_msgs_per_s` | Aggregate messages/second for the trial             |
| `mean_ns`               | Mean per-message round-trip latency (ns)            |
| `p50_ns`                | Median round-trip latency (ns)                      |
| `p95_ns`                | 95th-percentile round-trip latency (ns)             |
| `p99_ns`                | 99th-percentile round-trip latency (ns)             |
| `min_ns`                | Minimum observed round-trip latency (ns)            |
| `max_ns`                | Maximum observed round-trip latency (ns)            |
| `messages`              | Total messages across all processes in the trial    |
| `window_s`              | Longest per-process measurement window (s)          |

The CSV is convenient for plotting throughput (`throughput_msgs_per_s`) against
concurrency (`clients`) to visualize the saturation curve.

## Interpreting the results

- **Maximum message rate.** Plot mean throughput vs. `clients`. Throughput rises
  with concurrency and then flattens; the plateau is the daemon's maximum
  message rate. The knee of the curve indicates the concurrency at which the
  daemon's one-request-at-a-time processing becomes the bottleneck.

- **Latency under load.** The `p95`/`p99` columns show tail latency growing as
  the daemon saturates and requests begin to queue---expected for a closed-loop
  system past the knee.

- **Consistency check.** The tool computes `L = throughput_mean × mean_latency`
  and warns (and exits non-zero) if `L/N` leaves the range `0.70`–`1.10`. A
  large deviation suggests measurement error (e.g. windows not overlapping, or
  too little warmup). See the Little's Law note under
  [Methodology](#methodology).

### Caveats

- Percentiles are estimated from a base-2 log-spaced histogram: bucket `b`
  covers `[2^b, 2^(b+1))` ns, and each percentile is reported as that bucket's
  geometric midpoint. The estimate is therefore accurate only to within a
  bucket, i.e. up to roughly a factor of two.
- By default the transport is TCP over the loopback interface on a single node;
  numbers reflect that path and the host topology reported in the banner.
- The daemon's one-request-at-a-time `REP` socket is the design these numbers
  characterize, not a limit of ZeroMQ or the transport itself.

### Reproducibility

For repeatable runs, record the banner (host, processor count, versions, port,
window/warmup/trials) alongside the CSV. Also record the full launcher command,
including the process binding policy (e.g. `mpiexec -n N --bind-to core ...`),
since it affects the results and is not captured in the banner---see
[Process binding matters](#process-binding-matters). Pin the daemon and clients
to dedicated cores and isolate the node from other load. Use enough trials for
tight confidence intervals and a warmup long enough that the first timed trial
matches later ones.
