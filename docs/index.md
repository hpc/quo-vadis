---
layout: default
title: Home
---

Quo-Vadis (QV) is a next-generation, machine-independent coordination layer to
arbitrate access among multiple runtime components and map tasks efficiently to
heterogeneous architectures.

### Motivation

Scientific discovery is increasingly enabled by heterogeneous hardware that
includes multiple processor types, deep memory hierarchies, and heterogeneous
memories. To effectively utilize this hardware, computational scientists must
compose their applications using a combination of programming models,
middleware, and runtime systems. Since these systems are often designed in
isolation from each other, their concurrent execution results in resource
contention and interference, which limits application performance and
scalability. Consequently, real-world applications face numerous challenges on
heterogeneous machines.

Quo-Vadis is a runtime system that helps hybrid applications make efficient use
of heterogeneous hardware, eases programmability in the presence of multiple
programming abstractions, and enables portability across systems.


### Thread Support

Applications using OpenMP or POSIX threads can now benefit from Quo-Vadis'
high-level abstractions to map and remap full physics packages to the machine
dynamically with a handful of functions. Furthermore, the thread interface has
similar semantics to the process interface, allowing scientists to leverage a
single-semantics model for partitioning and assignment of resources to workers,
whether they are processes or threads. With both process and thread interfaces,
Quo-Vadis broadens its applicability to improve the productivity of
computational scientists across programming abstractions and heterogeneous
hardware.
