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
cmake -DQV_DISABLE_GPU_SUPPORT=ON -DQV_DISABLE_FORTRAN_SUPPORT=ON ..
```

| Option                       | Default | Comment                             |
| ---------------------------- | ------- | ----------------------------------- |
| QV_DISABLE_FORTRAN_SUPPORT   | OFF     | Disable Fortran support             |
| QV_DISABLE_GPU_SUPPORT       | OFF     | Disable GPU support                 |
| QV_DISABLE_MPI_SUPPORT       | OFF     | Disable MPI support                 |
| QV_DISABLE_OMP_SUPPORT       | OFF     | Disable OpenMP support              |
| QV_MPI_PROCESSES_ARE_THREADS | FALSE   | Affirm MPI processes are threads    |

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
# Set communication port
export QV_PORT=55996
# Launch the daemon
build/src/quo-vadisd &
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
