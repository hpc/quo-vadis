# quo-vadis

**This project is under active development and is currently not stable.**

A next-generation, machine-independent coordination layer to arbitrate access
among multiple runtime components and map workers efficiently to heterogeneous
architectures.

## Building
```shell
mkdir build && cd build && cmake .. && make
```
Or, using [ninja](https://ninja-build.org/), perform the following:
```shell
mkdir build && cd build && cmake -G Ninja .. && ninja
```

GPU device support is enabled by default. Define `QV_DISABLE_GPU_SUPPORT=ON` to
disable it. For example,
```shell
cmake -DQV_DISABLE_GPU_SUPPORT=ON ..
```

## Testing
Depending on the type of generator used perform either of the following:
```shell
make test # Or ninja test for ninja builds
```

## Environment Variables
```shell
QV_PORT # The port number used for client/server communication.
QV_TMPDIR # Directory used for temporary files.
```

For developers and debugging:
```shell
HWLOC_XMLFILE # Path to system topology XML file.
```
### Examples

```shell
export QV_PORT=55996

# Launch the daemon
$build_dir/src/quo-vadisd &

# Run a test
$build_dir/tests/test-scopes-mpi
```

## Internal Software Dependencies
* hwloc (https://github.com/open-mpi/hwloc)
* spdlog (https://github.com/gabime/spdlog)

## External Software Dependencies
* ZeroMQ (https://github.com/zeromq/libzmq)
* An MPI-3 implementation (https://www.open-mpi.org, https://www.mpich.org)
* OpenPMIx (https://github.com/openpmix/openpmix)

## Los Alamos National Laboratory Code Release
LA-CC-21-084
