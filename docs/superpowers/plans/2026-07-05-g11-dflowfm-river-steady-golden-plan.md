# G11 D-Flow FM River Steady Golden Plan

## Goal

Define the minimum executable-Golden path for G11 `dflowfm_river_steady`
without overstating current evidence.

## Current state

G11 currently has only boundary-readiness evidence:

1. runtime-loaded `DFlowFMEngine` exists in the main graph;
2. missing-library, missing-symbol, invalid-use, and fake-BMI positive-path unit
   coverage exists under `tests/unit/coupling/test_coupling_dflowfm_engine.cpp`;
3. D-Flow FM spike notes exist under `spikes/dflowfm/evidence/`, but the real
   external kernel + `.mdu` case run has not yet been recorded.

So G11 correctly remains:

- `status: pending`
- `ci_gate: false`

## Decision

Do NOT create an executable Golden under `tests/golden/dflowfm_river_steady/`
yet.

## Why

- A Golden executable should prove real runtime semantics, not only loader
  boundary behavior.
- The D-Flow FM adapter currently has no replayable real-kernel `.mdu` case in
  the repository.
- The current runtime adapter intentionally limits writes to scalar/location-0
  `set_var` until real shape inventory and `set_var_slice` evidence are known.
- Promoting a mock-only or fake-BMI-backed G11 executable would repeat the exact
  mismatch that G8 had to avoid before its real-runtime promotion.

## Minimum entry criteria for a real G11 executable Golden

Create `tests/golden/dflowfm_river_steady/` only when all of the following are
true:

1. A real D-Flow FM runtime library is available in CI or in a reproducible
   non-CI evidence environment.
2. A replayable `.mdu` case exists under `spikes/dflowfm/cases/` and documents:
   - target reach geometry / boundary setup;
   - expected steady-state variable names;
   - units;
   - required external files.
3. The spike has recorded a 100-step run that proves:
   - `initialize/update/finalize` lifecycle stability;
   - caller-supplied `dt` behavior;
   - variable inventory (`get_var_count/get_var_name`);
   - read/write semantics for the G11 variables;
   - pointer lifetime constraints for `get_var`;
   - representative fail-closed error behavior.
4. The executable Golden can assert steady-state behavior through the real
   adapter rather than a fake BMI DLL.
5. Fresh remote CI evidence exists if the PR changes manifest state.

## Recommended future test shape

When the runtime blocker is cleared, the G11 executable Golden should be scoped
narrowly:

- directory: `tests/golden/dflowfm_river_steady/`
- fixture source: authored `.mdu`-based river steady case with documented
  external file set
- assertions:
  - deterministic adapter initialization
  - stable current-time advance over repeated `update(dt)`
  - expected steady water level / discharge observation at named locations
  - no migration of `Q_limit`, deficit, replay, or arbitration semantics into
    `DFlowFMEngine`
- initial label state:
  - implemented, but still `ci_gate:false` until CI runtime availability is
    proven on both Linux and Windows

## Small next implementation PR after this plan

The next code-facing G11 PR should target the spike/evidence gap, not the Golden
manifest:

- add the real `.mdu` case and any required non-vendored runtime notes under
  `spikes/dflowfm/`
- record the real 100-step spike evidence
- only then add a real-runtime-backed `tests/golden/dflowfm_river_steady/`
  executable

## Expected final promotion PR

Once the executable Golden exists and is reproducible, the final promotion PR
should be small and change only:

- `tests/golden/dflowfm_river_steady/CMakeLists.txt`
- `tests/golden/suite_manifest/goldensuite.json`
- `tests/golden/suite_manifest/check_manifest.py`
- evidence docs with fresh CI references

and not bundle additional adapter/runtime refactors.
