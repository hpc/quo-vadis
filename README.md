# quo-vadis

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

## Testing
Depending on the type of generator used perform either of the following:
```shell
make test # Or ninja test for ninja builds
```

## Internal Software Dependencies
* hwloc (https://github.com/open-mpi/hwloc)
* nng (https://github.com/nanomsg/nng)
* spdlog (https://github.com/gabime/spdlog)

## External Software Dependencies
* ZeroMQ (https://github.com/zeromq/libzmq)
* An MPI-3 implementation (https://www.open-mpi.org, https://www.mpich.org)
* OpenPMIx (https://github.com/openpmix/openpmix)
