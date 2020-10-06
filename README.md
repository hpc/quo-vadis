# quo-vadis

A next-generation, machine-independent coordination layer to arbitrate access
among multiple runtime components and map workers efficiently to heterogeneous
architectures.

## Building
```shell
mkdir build && cd !$ && cmake .. && make
```
Or, using [ninja](https://ninja-build.org/), perform the following:
```shell
mkdir build && cd !$ && cmake -G Ninja .. && ninja
```

## Testing
Depending on the type of generator used perform either of the following:
```shell
make test
```

```shell
ninja test
```

## Internal Software Dependencies
* hwloc (https://github.com/open-mpi/hwloc)
* nng (https://github.com/nanomsg/nng)
* spdlog (https://github.com/gabime/spdlog)
