# G8 CI Gate Decision Plan

## Goal

Decide whether G8 `swmm_single_pipe_surcharge` should remain an implemented
non-gating Golden candidate or be promoted to `ci_gate:true` in GoldenSuite.

## Current state

G8 now has three layers of evidence:

1. Mock-boundary Golden candidate under `tests/golden/swmm_single_pipe_surcharge/`
2. Standalone real-SWMM spike evidence with authored `.inp` cases
3. Main-graph real `SwmmEngine` unit evidence through `test_coupling_swmm_engine`

However, the executable Golden itself still runs against mock-boundary logic, not
against the real SWMM runtime inside the GoldenSuite gate.

## Decision question

Should G8 be promoted to `ci_gate:true` now, or only after a real-SWMM Golden
runtime path exists in CI?

## Recommendation

Do NOT promote G8 to `ci_gate:true` yet.

## Why

- The current Golden test for G8 is still a mock-boundary surrogate.
- Real SWMM evidence exists, but it is split across spike execution and a main
  unit test, not the Golden executable itself.
- Promoting the gate now would overstate what the CI gate is actually proving.
- The stability protocol requires merge/release evidence to match the real gate
  semantics, not an adjacent evidence chain.

## Promotion entry criteria

Promote G8 to `ci_gate:true` only when all of the following are true:

1. A real-SWMM-backed G8 Golden executable exists under `tests/golden/`.
2. That Golden executable runs in both linux-gcc and windows-msvc CI jobs.
3. The test deterministically proves all G8 obligations:
   - surcharge/overflow path is real SWMM, not mock-only;
   - `Q_limit` clamp is exercised through the intended coupling seam;
   - deficit accounting / repayment assertions remain stable in CI;
   - authored `.inp` fixture and solver runtime are reproducible in CI.
4. Manifest/checker/CI labels are updated together in one PR.
5. Fresh remote CI evidence exists for the promotion PR.

## Small next implementation PR after this plan

Build the missing real-runtime Golden seam rather than changing the gate flag:

- add a new real-SWMM-backed G8 Golden executable under `tests/golden/`
  or migrate the current G8 Golden from mock boundary to real `SwmmEngine`;
- keep `ci_gate:false` during that runtime migration;
- once CI is green and semantics match the protocol, send a final tiny PR that
  changes only manifest/checker/CMake label state to promote G8.

## Expected output of the future promotion PR

A very small PR that updates:

- `tests/golden/swmm_single_pipe_surcharge/CMakeLists.txt`
- `tests/golden/suite_manifest/goldensuite.json`
- `tests/golden/suite_manifest/check_manifest.py`
- evidence docs with fresh CI run references

and nothing else substantial.
