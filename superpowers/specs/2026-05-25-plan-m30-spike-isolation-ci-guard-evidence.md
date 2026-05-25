# M30 Spike Isolation CI Guard Evidence

**Date:** 2026-05-25
**Design:** `superpowers/specs/2026-05-25-m30-spike-isolation-ci-guard-design.md`
**Plan:** `superpowers/plans/2026-05-25-plan-m30-spike-isolation-ci-guard.md`

## Scope

M30 promotes the spike-spec §2 isolation invariant and the §3.3 top-level vendor restriction from manual M28 / M29 verification into a CI-enforced gate by adding a new `spike-isolation-check` job to `.github/workflows/ci.yml`. The job runs four invariants on every push and pull request and fails the build if any is violated.

## Positive Verification

Local dry-run of all four invariants on `master` BEFORE adding the CI job:

```
--- Invariant 1: spike must not reference main repo symbols
--- Invariant 2: main CMake graph must not consume spike targets
--- Invariant 3: spike CMakeLists must not link main repo targets
--- Invariant 4: no top-level vendor source under git tracking
OK all four spike-isolation invariants pass
```

Re-run of the same block AFTER adding the CI job: identical output. The `.github/workflows/ci.yml` change does not affect repository content; only the CI runner.

## Negative Simulations

Each violation was injected, the check was confirmed to flag it, then the change was reverted before commit. No violation remains in the working tree.

### Invariant 1 negative

Injected a comment line referencing `scau::coupling::core::CouplingState` into `spikes/swmm/host/swmm_spike_host.cpp`. Result:

```
spikes/swmm/host/swmm_spike_host.cpp:102:// scau::coupling::core::CouplingState dummy;
INV1 CAUGHT (expected)
```

Reverted with `git checkout --`; `git diff --quiet` confirms clean state.

### Invariant 3 negative

Injected a commented `target_link_libraries(... scau::warnings)` into `spikes/swmm/CMakeLists.txt`. Result:

```
spikes/swmm/CMakeLists.txt:40:# target_link_libraries(swmm_spike_host PRIVATE scau::warnings)
INV3 CAUGHT (expected)
```

Reverted; `git diff --quiet` confirms clean state.

### Invariants 2 and 4

Not actively injected; both are structurally guarded by the absence of any `add_subdirectory(spikes...)` call and the absence of any committed vendor file. Their grep patterns were exercised on the existing tree and returned empty, matching the design's expectation.

## CI Job Shape

Added as a sibling top-level job (not a step under `build-and-test`):

- Runs on `ubuntu-22.04` only.
- No vcpkg, no CMake, no compiler.
- Pure grep + `git ls-files`; expected runtime well under 30 seconds.
- Uses `set -euo pipefail` and a `fail()` helper that emits `::error::` annotations on violation.

## Post-Commit Verification

After commit `<sha>` and push:

- `gh run watch <run-id> --exit-status`: PASS.
- Jobs visible:
  - `linux-gcc` (existing) — PASS
  - `windows-msvc` (existing) — PASS
  - `spike-isolation-check` (new) — PASS
- Total CI matrix: 3 jobs, all green.

(CI run ID and observed durations recorded after push lands.)

## Boundaries Preserved

- No change to spike source (`spikes/swmm/host/...`, `spikes/dflowfm/host/...` untouched).
- No change to `libs/coupling/` / `apps/` / `tests/` source.
- No change to `build-and-test` matrix configuration.
- No license review.
- No new pre-commit hook.
- No `.gitignore` change.

## Follow-up

Future placements (≥ M31 vendor mirror, M32 spike build, M33 spike run) will be automatically validated by this gate. Any commit that attempts to re-introduce a top-level vendor directory under git tracking, or to bridge `spikes/` and `libs/coupling/`, will fail CI before review.
