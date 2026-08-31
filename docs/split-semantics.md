---
layout: default
title: Split Semantics
---

One of the fundamental and powerful operations in Quo Vadis is the `split` operation. This *collective* operation partitions a set of hardware resources into pieces and assigns each calling process to one or more pieces.

## Two types of split operations

The are are two types of split operations: the *regular* operation and the *device-based* operation. The resources to split are encapsulated into the input scope and the resulting resource subset per process is encapsulated into the output subscope.

```C
// Regular split
int
qv_split(qv_scope_t *in_scope, int num_pieces, int color,
         qv_scope_t **out_subscope);
```

The regular split takes an *input scope* and divides it into the desired *number of pieces*. Each process provides a *color*, an ordinal indicating which piece this process should be assigned to. This results in each process being assigned to exactly one piece, which is encapsulated into a subscope.

This operation focuses on partitioning the most abundant resource, usually Cores, and creating the appropriate subscopes. In addition to Cores, each subscope includes other resources such as memory and GPUs that are close to the given Cores, but maintaining hardware locality is not guaranteed.

```C
// Device-based split
int
qv_split_at(qv_scope_t *in_scope, qv_hw_obj_type_t device_type, int color,
            qv_scope_t **out_subscope);
```

Similarly, the device-based split takes an *input scope*, but the number of resulting pieces is derived based on the *device type* parameter. For example, if the type is NUMA memory and the input scope has eight NUMAs, then eight pieces are created, each having local resources associated with that memory (e.g., cores and GPUs). Like the regular split, each process is assigned to exactly one piece, which is encapsulated into a subscope.

This operation focuses on creating as many pieces as devices and guarantees that all of the resources associated with a piece are local to the associated device. In other words, this operation *maintains hardware locality*.

```C
// Commonly used device types
QV_HW_OBJ_NUMANODE;
QV_HW_OBJ_GPU;
QV_HW_OBJ_CORE;
QV_HW_OBJ_PU;
QV_HW_OBJ_NIC;
```

## Implicit colors

In addition to specifying an explicit color as an ordinal, Quo Vadis provides implicit colors that spare the user from having to calculate a color for each process. These are *Auto*, *Packed*, and *Spread*.


```C
QV_SPLIT_AUTO;
QV_SPLIT_PACKED;
QV_SPLIT_SPREAD;
```

As mentioned above, the color specifies the mapping of processes to pieces. The semantics of these implicit colors depend on whether there are more processes than pieces and viceversa. Unlike an ordinal color, using an implicit color may result in at least one process having more than one piece in its associated subscope. In addition, implicit colors can be used on both the regular split and the device-based split.

### Auto (Entire-Block)

This color assings *all* of the pieces to processes, leaving no piece unassigned. Its semantics are as follows.

`Num. processes < num. pieces`<br>
Since the number of processes is stricly less than the number of pieces, a one-to-one mapping leaves pieces unassigned. The auto color assigns *all* of the pieces (the *entire* resources) to processes, which results in at least one process having more than one piece. Thus, at least one output subscope includes more than one piece.

`Num. processes >= num. pieces`<br>
Distributes the processes over the pieces as evenly as possible in a *block* manner. At least one process is assigned to more than one piece, which is encapsulated in its output subscope.


### Packed-Block

Each process is assigned to exactly one piece.

`Num. processes < num. pieces`<br>
*Pack* `n` processes into the first `n` contiguous pieces, one piece per process. There is at least one piece that remains unassigned.

`Num. processes >= num. pieces`<br>
Distribute the processes over the pieces in a *block* manner. It distributes the processes as evenly as possible over the pieces so that processes `0 - n1` are assigned to piece `0`, processes `n1+1 - n2` are assigned to piece `1`, and so forth. As a result, processes assigned to the same piece are contigous.


### Spread-Cyclic

Each process is assigned to exactly one piece.

`Num. processes < num. pieces`<br>
*Spread* the processes over the pieces, one piece per process, maximizing the distance between the assigned pieces. There is at least one piece that remains unassigned.

`Num. processes >= num. pieces`<br>
Distribute the processes over the pieces in a *cyclic* manner. It assigns processes to pieces starting with piece `0`, then piece `1`, and so forth, until all of the processes have been assigned.








