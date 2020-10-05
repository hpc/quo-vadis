# quo-vadis

A next-generation, machine-independent coordination layer to arbitrate access
among multiple runtime components and map workers efficiently to heterogeneous
architectures.

## Building
```
mkdir build && cd !$ && cmake .. && make
```

## Internal Software Dependencies
* hwloc (https://github.com/open-mpi/hwloc)
* nng (https://github.com/nanomsg/nng)
* spdlog (https://github.com/gabime/spdlog)
