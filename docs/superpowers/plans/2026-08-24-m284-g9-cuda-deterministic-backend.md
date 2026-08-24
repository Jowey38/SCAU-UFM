# M284: G9 deterministic CUDA backend (M266 contract, M280 pattern)

Date: 2026-08-24
Status: IN PROGRESS

## Scope

Implement `BackendKind::cuda_deterministic` for the Surface2D step behind the
M266 fail-closed backend contract, following the M280-proven deterministic
pattern (gather-by-owner in fixed per-cell edge order, no atomics; fixed-order
block-tree reductions with sequential host combination; double precision;
`--fmad=false` so device arithmetic contracts match the /fp:precise host).
`cuda_performance` stays unavailable.

## Design

1. Shared numerics, one source of truth: throw-free pure kernels
   (`hllc_normal_flux`, `reconstruct_hydrostatic_pair`,
   `well_balanced_edge_pairing`, `classify_edge`, phi_t pairing) become
   `SCAU_HD inline` header implementations compiled identically into the CPU
   library and the CUDA backend. Validating kernels (friction, coupling
   exchange, CVC, wetting/drying, Green-Ampt, ground runoff) split into a
   host-side checked wrapper (unchanged behavior) plus an `SCAU_HD` unchecked
   core; the CUDA path re-creates the same failure surface with host
   pre-validation plus a device error flag, restoring the device snapshot
   before throwing.
2. Determinism: per-edge kernel computes fluxes/WB/CVC and writes per-side
   contribution slots; per-cell kernel gathers its owned edges in ascending
   global edge index (bitwise-identical association to the CPU edge loop).
   Scalar volume diagnostics use the M280 fixed-order 256-wide block tree with
   sequential host combination (deterministic; vs CPU sequential sums within
   the locked 1e-12 tolerance). `max_cell_cfl` gathers per cell then takes an
   exact max.
3. State h and eta match the CPU bitwise; hu/hv match bitwise except through
   Manning friction, where device `pow` may differ by ulps (locked at 1e-12).
4. Roof stage keeps the CPU implementation between device stages: its
   CouplingLib acceptance callback (`RoofDrainageAcceptanceFn`) is an external
   coupling seam that can never be a device function. Ground runoff
   (Green-Ampt) runs on device.
5. Snapshot/restore: device-to-device buffer copies before mutation; error
   paths restore before throwing; a public roundtrip evidence hook backs the
   G9 snapshot fixture.
6. Build gating: root `option(SCAU_ENABLE_CUDA OFF)`; when ON the surface2d
   target gains the `.cu` backend, `CUDA_ARCHITECTURES` from
   `SCAU_CUDA_ARCHITECTURES` (default 61 for the governed Quadro P2200), and
   a PUBLIC `SCAU_SURFACE2D_HAS_CUDA` define. Default OFF keeps hosted CI
   (Linux GCC, no CUDA) exactly as before; capabilities stay fail-closed.

## G9 fixture matrix (M266)

One golden `tests/golden/cpu_gpu_deterministic_match` runs CPU and CUDA
backends over: (1) lake at rest; (2) static phi_t jump; (3) Phi_c/phi_e_n
jump; (4) hard/soft blockage; (5) wet/dry and near-dry donor; (6) rainfall,
infiltration, friction; (7) coupling exchange and Q_limit-applied volume;
(8) runoff and roof overflow; (9) CVC flag off and on; (10) rollback/replay
and diagnostics reset; (11) file-driven mixed tri/quad STCF case. Every
fixture records h/hu/hv/eta, raw max_cell_cfl, volume diagnostics and
rollback state; repeated CUDA runs must be bitwise identical; snapshot
restore must be bitwise. The golden skips (GTEST_SKIP) when the CUDA backend
is not compiled or no device is present, exactly like the real-engine
goldens; the governed GPU host is the enforcing execution.

## Exit conditions tracked against M266

- real CUDA deterministic implementation: this plan.
- matrix within 1e-12 or stricter: G9 assertions (bitwise for h/eta always).
- repeated CUDA result deterministic: bitwise repeat fixture.
- device snapshot/restore evidence: roundtrip fixture.
- blocking local GPU runner + recorded toolkit/driver/device: local ctest on
  the governed host (Quadro P2200, driver 571.59, CUDA 12.8.1); CI lane on
  the self-hosted runner is a follow-up recorded in the evidence doc.
- manifest flip pending -> implemented only after all of the above pass.
