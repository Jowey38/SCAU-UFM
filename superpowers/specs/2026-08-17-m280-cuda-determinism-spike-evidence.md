# M280 CUDA Determinism Spike Evidence

Date: 2026-08-17
Scope: G9 (`cpu_gpu_deterministic_match`) entry de-risk per the M266 backend
contract. Spike-only; no main-graph CUDA code, no backend availability
change, no G9 status change.

## Toolchain provisioning (M266 blocker resolved)

- GPU: NVIDIA Quadro P2200 (compute capability 6.1, 5 GB), driver 571.59
  (CUDA driver version 12080).
- CUDA Toolkit 13.3 was installed first and REJECTED for this host: nvcc 13.x
  dropped Pascal (minimum compute_75) and the 12.8-level driver cannot run
  13.x binaries. CUDA Toolkit 12.8.1 (nvcc/cudart/VS integration components)
  installed side-by-side; nvcc 12.8 targets compute_50+ including sm_61.
- End-to-end smoke: double-precision axpy kernel compiled with
  `-arch=sm_61` and executed correctly on the device.

## Determinism evidence (spikes/cuda/determinism)

Solver-shaped fixture: 1,048,576 cells x 4 owned edge slots, 25 update steps
(nonlinear per-cell update from per-edge contributions in a fixed order,
gather-by-owner, no atomics), whole-domain storage via a fixed-order block
tree (256-wide sequential pairwise) with block partials combined
sequentially on the host.

- `[repeat] runs=5 bitwise_identical=true` — repeated device runs are
  bitwise identical (state vector AND storage).
- `[cpu-match] state_bitwise=true storage_bitwise=true` — the device result
  is bitwise equal to a CPU reference replicating the same association
  order (storage=1049099.7680197724).

## Consequence for G9

The two G9 entry risks are closed on the governed host: the toolchain
exists, and the deterministic pattern (gather-by-owner + fixed-order
reduction, double precision) is proven bitwise-reproducible and
CPU-matchable on real hardware. The remaining G9 work is the actual solver
port (HLLC/WB/DPM/source terms/wet-dry/CVC/rollback snapshot) following
this exact pattern, fixture matrix per M266, with the backend contract kept
fail-closed until every exit condition passes. G9 stays `pending` /
`ci_gate:false`.
