# M284 G9 Deterministic CUDA Backend Evidence

Date: 2026-08-24
Scope: G9 `cpu_gpu_deterministic_match` implementation per the M266 backend
contract, following the M280-proven deterministic pattern. Fresh local
blocking evidence on the governed GPU host.

## Toolchain / device (recorded by the golden itself)

- GPU: NVIDIA Quadro P2200 (compute capability 6.1), driver 571.59
  (CUDA driver version 12080), runtime 12080.
- CUDA Toolkit 12.8 (nvcc 12.8.93) selected via the VS toolset
  (`-T cuda=12.8`); 13.3 remains rejected for this host (M280).
- MSVC 19.44 host compiler; device compiled `--fmad=false` so contraction
  matches the /fp:precise host.
- M280 spike re-compiled and re-run fresh on this host before the port:
  `runs=5 bitwise_identical=true`, `state_bitwise=true storage_bitwise=true`.

## Implementation shape

- Shared SCAU_HD numerics: HLLC, Audusse reconstruction, WB pairing, edge
  classification, phi_t pairing, boundary ghosts are single-source
  header-inline for both backends; validating kernels split into checked host
  wrappers + unchecked SCAU_HD cores (identical arithmetic). CPU reference is
  bitwise unchanged (full suite green before any CUDA code).
- Deterministic pattern: per-edge kernel -> per-cell gather in ascending
  global edge index (bitwise CPU association; no atomics); fixed-order
  256-wide block-tree reductions with sequential host combination for scalar
  volume diagnostics; exact max/count reductions.
- Ground runoff (Green-Ampt) runs on device; the roof stage runs the exact
  CPU function between device stages because its CouplingLib acceptance port
  (std::function) is an external coupling seam by design.
- Fail-closed: `SCAU_ENABLE_CUDA` defaults OFF; capabilities report
  cuda_deterministic available only when compiled AND a device is present;
  cuda_performance stays unavailable; unavailable requests throw before
  state mutation. The CUDA step commits the caller's state only after every
  stage succeeded (atomic per step; the device error flag re-creates the
  checked-kernel failure surface).

## G9 results (local blocking ctest, Debug, 2026-08-24)

- Fixture matrix (M266 items 1-11 plus a boundary-kinds fixture): CPU vs
  CUDA — h, eta, per-cell residuals, all 9 per-edge diagnostics, raw
  `max_cell_cfl`, rollback state: BITWISE equal (EXPECT_EQ). hu/hv bitwise on
  all fixtures except Manning-friction-active (1e-12 lock, device pow ulps).
  Accumulated volume diagnostics within 1e-12. RunoffState arrays bitwise.
- Repeated CUDA runs: bitwise identical state (memcmp) across 3 runs per
  fixture.
- Device snapshot/restore roundtrip: bitwise identical after device-side
  perturbation and restore.
- Full suite with CUDA ON: 162/162 (manifest gate green).
- Full suite with CUDA OFF (hosted-CI equivalent): 162/162 after clearing a
  stale-exe LNK1168 episode (fresh binaries verified, LNK1168 count 0);
  G9 golden reports the explicit SKIP path.

## M266 exit conditions

| Condition | Status |
|---|---|
| Real CUDA deterministic implementation | DONE (this slice) |
| Matrix within 1e-12 or stricter | DONE (bitwise for h/eta/diagnostics; 1e-12 locks recorded in tolerances.md) |
| Repeated CUDA result deterministic | DONE (bitwise repeat fixture) |
| Device snapshot/restore evidence | DONE (roundtrip fixture) |
| Blocking GPU runner + recorded toolkit/driver/device | Local blocking ctest on the governed host; device identity recorded by the golden. A CI lane on the self-hosted runner is the remaining follow-up. |
| Manifest pending -> implemented after all pass | G9 flipped to `implemented`, `ci_gate:false` |

## Governance

G9 stays `ci_gate:false` until the self-hosted runner executes the golden as
a blocking lane (same promotion pattern as G11/G16-G18: three consecutive
green runs before `ci_gate:true`). Do not claim release-gate coverage from
the local evidence alone.
