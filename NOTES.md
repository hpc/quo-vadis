# NOTES

Scopes
======
* A scope describes a collection of hardware resources.
* Scopes can dynamically 'change shape' during execution, and have transferable
  ownership, e.g.,  a parent can ask for n scopes and distribute them as needed.
  This will help with dynamic load balancing, for example.
* A scope modification call can fail. For example, asking for more resources.

Hardware Topologies
===================
* We may have to add additional capabilities to support GPUs. It may be that
  hwloc_topology_restrict() cannot support what we might need.
* We probably don't need multiple topologies. Just use scopes!
