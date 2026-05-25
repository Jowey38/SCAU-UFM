# M29 Third-Party Spike Host Skeleton Evidence

**Date:** 2026-05-25
**Design:** `superpowers/specs/2026-05-25-m29-third-party-spike-host-skeleton-design.md`
**Plan:** `superpowers/plans/2026-05-25-plan-m29-third-party-spike-host-skeleton.md`

## Scope

M29 creates the structural skeleton of SWMM 5.2.4 and D-Flow FM BMI 1.0 spike
host programs and their isolated CMake scaffolding under `spikes/swmm/` and
`spikes/dflowfm/`, with host source aligned to the real third-party API
shapes observed in:

- `Stormwater-Management-Model-develop/src/solver/include/swmm5.h`
- `Delft3D-main/src/engines_gpl/dimr/packages/dimr_lib/include/bmi.h`

No vendor source mirrored. No spike binary built or run. Main CMake graph
untouched.

## Files Created

### SWMM spike (7 files)

- `spikes/swmm/CMakeLists.txt`
- `spikes/swmm/host/swmm_spike_host.cpp`
- `spikes/swmm/cases/README.md`
- `spikes/swmm/cases/README_manhole_overflow.md`
- `spikes/swmm/evidence/abi_gotchas.md`
- `spikes/swmm/evidence/interface_gap_matrix.md`
- `spikes/swmm/evidence/spike_report.md`

### D-Flow FM spike (7 files)

- `spikes/dflowfm/CMakeLists.txt`
- `spikes/dflowfm/host/dflowfm_spike_host.cpp`
- `spikes/dflowfm/cases/README.md`
- `spikes/dflowfm/cases/README_reach_with_weir.md`
- `spikes/dflowfm/evidence/abi_gotchas.md`
- `spikes/dflowfm/evidence/interface_gap_matrix.md`
- `spikes/dflowfm/evidence/spike_report.md`

## Gap Inventory (Seeded From Header Inspection)

### SWMM 5.2.4

| Gap | Symbol | Severity | Real API actually present |
|---|---|---|---|
| S1 | `swmm_setNodeInflow` (spike pseudocode) | MATCH_WITH_NOTE | `swmm_setValue(swmm_NODE_LATFLOW=307, idx, q)` |
| S2 | caller-supplied dt via `swmm_step` | GAP_BLOCKING ? | `swmm_step(double *elapsedTime)` — elapsedTime is OUTPUT |
| S3 | `swmm_saveHotStart` | GAP_BLOCKING ? | not in public API; `swmm_start(saveFlag=1)` writes `.hsf` only |
| S4 | second instance in same process | GAP_BLOCKING | precluded by file-scope globals in `swmm5.c` |

### D-Flow FM BMI 1.0

| Gap | Symbol | Severity | Real API actually present |
|---|---|---|---|
| D1 | `bmi::Bmi*` class handle | GAP_BLOCKING | API is free C functions; process-global |
| D2 | `update_until(target_time)` | MATCH_WITH_NOTE | `update(double dt)` — verify engine respects caller dt |
| D3 | `set_var` / `get_var` ownership | MATCH_WITH_NOTE | set: caller-owned buffer; get: engine-owned pointer (caller must NOT free) |
| D4 | `save_state(path)` | GAP_BLOCKING ? | not in BMI 1.0 header |
| D5 | variable identification | MATCH | string name; enumerated via `get_var_count` + `get_var_name` |
| D6 | RTC / weir / gate state | GAP_MITIGABLE ? | not in BMI 1.0 base; likely available via D-Flow FM-specific headers |

## Verification

- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, 31/31 (10.29s). Main CMake graph unchanged; CMake did not reconfigure.
- `grep -r "scau::coupling" spikes/`: no matches. Spike has no dependency on `libs/coupling/`.
- `grep -r "libs/coupling" spikes/`: no matches. Spike does not reach into the main repo source tree.
- `grep -r "add_subdirectory" spikes/`: only matches are comments inside both spike `CMakeLists.txt` explicitly forbidding `add_subdirectory(this)` from the main repo. No actual `add_subdirectory()` invocations exist.

## Boundaries Preserved

- No vendor source mirrored into `spikes/<engine>/vendor/`.
- No `.inp` / `.mdu` real input files placed; only README placeholders documenting required content.
- No spike binary built or run.
- No main-repo CMake graph change.
- No connection between `spikes/` and `libs/coupling/`.
- No license review.
- No third-party source committed; M28 `.gitignore` guard against top-level vendor dirs remains active.

## Follow-up Milestones

M29 produces only the skeleton. To complete the spike-spec §7 exit criteria,
the following work is required in subsequent milestones:

- M30: vendor placement decision + AGPL/GPL license review for `Delft3D-main/`.
- M31: build the spike hosts against real vendor checkouts; resolve `SWMM_SOURCE_DIR` / `DFLOWFM_SOURCE_DIR` to real paths; produce the first `.hsf` / `.mdu` test cases.
- M32: run the spike hosts; populate `evidence/spike_report.md` `TBD` fields with replayable evidence.
- M33: write each `GAP_BLOCKING` finding back to main-Spec §7 as redesign proposals; obtain Phase 1 entry gate.
