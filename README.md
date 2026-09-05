[![QA](https://github.com/hpc/quo-vadis/actions/workflows/qa.yml/badge.svg)
](https://github.com/hpc/quo-vadis/actions/workflows/qa.yml)

# quo-vadis

**This project is under active development and is currently not stable.**

A next-generation, machine-independent coordination layer to arbitrate access
among multiple runtime components and map tasks efficiently to heterogeneous
architectures.

## Building
```shell
mkdir build && cd build && cmake .. && make
```
Or, using [ninja](https://ninja-build.org/), perform the following:
```shell
mkdir build && cd build && cmake -G Ninja .. && ninja
```

### Build Options
Below is a table of build options for quo-vadis (QV). Options can be specified
in a variety of ways using `cmake` or `ccmake`. For example,
```shell
cmake -DQV_GPU_SUPPORT=OFF -DQV_FORTRAN_SUPPORT=OFF ..
```

| Option                       | Default | Comment                             |
| ---------------------------- | ------- | ----------------------------------- |
| QV_FORTRAN_SUPPORT           | ON      | Toggle Fortran support              |
| QV_GPU_SUPPORT               | ON      | Toggle GPU support                  |
| QV_MPI_SUPPORT               | ON      | Toggle MPI support                  |
| QV_OMP_SUPPORT               | ON      | Toggle OpenMP support               |


### Developer Build Options
Below is a table of build options for quo-vadis developers. Options can be
specified in a variety of ways using `cmake` or `ccmake`. For example,
```shell
cmake -DQV_SANITIZE=address ..
```

| Option              | Default | Comment                                      |
| ------------------- | ------- | -------------------------------------------- |
| QV_SANITIZE         | none    | Available: address;thread;undefined          |
| QV_DEVELOPER_MODE   | depends | If in repo, ON by default; OFF otherwise     |

## Developer Documentation
See [Git Workflow](docs/git-workflow.md) for our typical branching, pull
request, and rebasing conventions.

## Testing
```shell
ctest
# Or target a specific suite of tests
ctest -L core
# Or verbosely
ctest -V
```

## Benchmarking
A micro-benchmark suite under `tests/benchmarks/` measures the average (plus
min/max) execution time of the public functions exposed by `quo-vadis.h`,
`quo-vadis-thread.h`, and `quo-vadis-mpi.h`. Functions that operate on a
`qv_scope_t` are benchmarked by a shared driver, so the process, thread, and
MPI suites reuse the same measurement and reporting code.

The benchmarks are registered with `ctest` under the `benchmark` label and run
against a `quo-vadisd` daemon (started automatically, just like the tests):
```shell
# Run all benchmarks.
ctest -L benchmark -V
# Or a single scope kind (process, thread, or mpi).
ctest -R bench-process -V
ctest -R bench-thread -V
ctest -R bench-mpi -V
```
The benchmark binaries (`qvb-process`, `qvb-thread`, `qvb-mpi`) can also be run
directly once a daemon is available; the MPI benchmark is launched with
`mpiexec` and only rank 0 prints results:
```shell
# Launch the daemon (see Examples below), then:
./build/tests/benchmarks/qvb-process
mpiexec -n 2 ./build/tests/benchmarks/qvb-mpi
```

The number of timed iterations per function defaults to 10 and can be
overridden with the `QVB_ITERS` environment variable:
```shell
QVB_ITERS=1000 ctest -R bench-process -V
```

Only performance numbers from `Release` builds should be cited. In-repo builds
default to `Debug` with developer flags enabled, which are not representative of
real-world performance; configure a `Release` build before collecting numbers to
report:
```shell
cmake -DCMAKE_BUILD_TYPE=Release ..
```

## Environment Variables
```shell
QV_PORT # The port number used for client/server communication.
QV_TMPDIR # Directory used for temporary files.
QV_VEXCEPT # When set to any value provides verbose exception output.
QV_VMAP # When set to any value provides verbose mapping output.
```

For developers and debugging:
```shell
HWLOC_XMLFILE # Path to system topology XML file.
```
### Examples
```shell
# Launch the daemon with specified port.
build/src/quo-vadisd --port 55996
# Run a test.
./build/tests/test-process-scopes
```

QV supports both manual and automatic `quo-vadisd` startup for MPI applications.
For this to work, the path to `quo-vadisd` must be in your `PATH`.
```shell
# Set PATH to location of quo-vadisd. For installs
# this will commonly be set to CMAKE_INSTALL_PREFIX/bin.
PATH=$PWD/build/src:$PATH
# Optionally set the communication port via environment variable.
# If QV_PORT is not set, then QV will chose a default port for you.
# Run a test.
mpiexec -n 2 build/tests/test-mpi-scopes
```

## Internal Software Dependencies
* hwloc (https://github.com/open-mpi/hwloc)
* cereal (https://github.com/USCiLab/cereal)
* spdlog (https://github.com/gabime/spdlog)

## External Software Dependencies
* Required
    * ZeroMQ (https://github.com/zeromq/libzmq)
* Optional
    * An MPI-3 implementation (https://www.open-mpi.org, https://www.mpich.org)

## Packaging for Developers
```shell
# To generate source distributions, run the following:
git branch -m roll-release
# Modify CMakeLists.txt to change release version.
git commit -a -m "Roll a release"
git clone $PWD release
mkdir release/build
cd release/build
cmake ..
make package_source
```

## Los Alamos National Laboratory Code Release
LA-CC-21-084
