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
Below is a table of build options for quo-vadis. Options can be specified in a
variety of ways using `cmake` or `ccmake`. For example,
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


## Testing
```shell
ctest
# Or target a specific suite of tests
ctest -L core
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
# Set communication port
export QV_PORT=55996
# Launch the daemon
build/src/quo-vadisd
# Run a test
mpiexec -n 2 build/tests/test-mpi-scopes
```

## Internal Software Dependencies
* hwloc (https://github.com/open-mpi/hwloc)
* spdlog (https://github.com/gabime/spdlog)

## External Software Dependencies
* ZeroMQ (https://github.com/zeromq/libzmq)
* An MPI-3 implementation (https://www.open-mpi.org, https://www.mpich.org)

## Packaging for Developers
```shell
# To generate source distributions, run the following:
cd build
make package_source
```

## Los Alamos National Laboratory Code Release
LA-CC-21-084
