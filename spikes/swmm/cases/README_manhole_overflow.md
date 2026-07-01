# manhole_overflow.inp specification

`manhole_overflow.inp` is authored in this repository for the G8 real-SWMM
evidence path. It is not copied from EPA or third-party example sets.

Case shape:

- `J0`: upstream inflow junction receiving constant `TS_SURCHARGE` flow.
- `J1`: shallow manhole (`MaxDepth = 0.35 m`) intended to expose head rise and
  overflow/surcharge behaviour.
- `C0`: upstream conduit from `J0` to `J1`.
- `C1`: restricted downstream conduit from `J1` to free outfall `OUT1`.
- `OUT1`: free outfall.

Evidence target:

- run through `spikes/swmm/host/swmm_spike_host.cpp`;
- observe real SWMM `swmm_NODE_HEAD` and `swmm_NODE_OVERFLOW` at node `J1`;
- archive the run output as G8 physical evidence before any future promotion of
  `swmm_single_pipe_surcharge` to `ci_gate:true`.

This remains standalone spike evidence until real SWMM is wired into the main
CMake/test graph.
