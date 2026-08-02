# M266 CUDA Backend Contract and G9 Matrix

Date: 2026-07-30

## Status

The current environment has no `nvcc`; no CUDA implementation or G9 pass is
claimed. This slice defines the fail-closed backend boundary needed before CUDA
code can enter the numerical core.

## Contract

- `cpu_reference`: available, deterministic, double precision, snapshot-ready.
- `cuda_deterministic`: unavailable until real double kernels, fixed reduction,
  device-state snapshot and a blocking GPU runner exist.
- `cuda_performance`: unavailable until deterministic CUDA is the reference;
  atomics/CUDA Graphs can never be the sole correctness path.

A request for either unavailable CUDA mode throws before state mutation.

## G9 fixture matrix

A future G9 must compare CPU reference and deterministic CUDA for:

1. hydrostatic lake at rest;
2. static `phi_t` jump;
3. `Phi_c/phi_e_n` jump;
4. hard/soft blockage;
5. wet/dry and near-dry donor;
6. rainfall, infiltration and friction;
7. coupling exchange and `Q_limit`-applied volume;
8. runoff and roof overflow;
9. CVC flag off and on;
10. rollback/replay and diagnostics reset;
11. file-driven mixed tri/quad STCF case.

Every fixture records h/hu/hv/eta, raw `max_cell_cfl`, volume diagnostics and
rollback state. The debug correctness path uses double and fixed reduction.

## Exit conditions for G9

- real CUDA deterministic implementation;
- all matrix fixtures within 1e-12 or stricter locked tolerance;
- repeated CUDA result deterministic;
- device snapshot/restore evidence;
- blocking local GPU runner and recorded toolkit/driver/device;
- manifest changes from pending to implemented only after all above pass.
