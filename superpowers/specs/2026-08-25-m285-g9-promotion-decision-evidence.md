# M285 G9 Promotion Decision Evidence

Date: 2026-08-25
Decision: **promote G9 `cpu_gpu_deterministic_match` to an active GoldenSuite
gate (`ci_gate: true`)** after three consecutive green blocking runs of the
`gpu-cuda-golden` self-hosted lane, following the G11/G16-G18 and G20 (M283)
promotion pattern.

## Lane

Added by PR #81 (master 77a5298): job `gpu-cuda-golden`, runner labels
`[self-hosted, windows, cuda]` (governed host SCAU-DFLOWFM-SERVER-01, NVIDIA
Quadro P2200, CUDA Toolkit 12.8 pinned via the `windows-msvc-cuda` preset VS
toolset; 13.3 remains rejected for this host per M280/M284). The job
configures with `SCAU_ENABLE_CUDA=ON`, builds the G9 golden target and runs
it blocking via ctest. Gated on repository variable `SCAU_CUDA_RUNNER` and
vcpkg root `SCAU_CUDA_VCPKG_ROOT`, mirroring the `real-dflowfm-golden`
pattern.

## Three consecutive green runs (stability protocol: represented-in-CI)

| # | Run | Trigger | gpu-cuda-golden result |
|---|---|---|---|
| 1 | 32795992046 (PR #81) | pull_request | success (23m39s, first build of the lane workspace) |
| 2 | 32799603158 (master 77a5298) | push | success (22m21s) |
| 3 | 32810975939 (master 77a5298) | workflow_dispatch | success |

The golden itself records the governed device identity and enforces the M266
matrix: 12-fixture CPU/CUDA bitwise match (h, eta, per-cell residuals, all
per-edge diagnostics, raw `max_cell_cfl`, rollback state; hu/hv bitwise
except the Manning fixture's documented 1e-12 lock), bitwise repeated-run
determinism, and the device snapshot/restore roundtrip (M284 evidence).

## Synchronized promotion edits (this change)

- `tests/golden/suite_manifest/goldensuite.json`: G9 `ci_gate` false -> true.
- `tests/golden/suite_manifest/check_manifest.py`: REQUIRED table G9 -> True.
- `tests/golden/cpu_gpu_deterministic_match/CMakeLists.txt`: labels
  `golden_candidate;cuda;surface2d` -> `golden;cuda;surface2d` so the hosted
  and self-hosted `-L golden` gates include the test. Hosted CUDA-off builds
  take the golden's explicit SKIP path (M284 evidence: full suite green with
  CUDA off), so hosted lanes remain meaningful without a device.

## Consequence

- G9 joins the active GoldenSuite gates; failures block merge per the
  stability protocol.
- With this promotion the M266 exit conditions are fully closed (the last
  open item was the blocking GPU runner lane).
- Every remaining non-gating manifest entry (G15, G26) is superseded-by-design
  (real_both/G19 scope), not evidence-pending.
