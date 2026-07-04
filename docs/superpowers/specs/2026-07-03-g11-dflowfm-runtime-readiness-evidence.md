# G11 D-Flow FM Runtime Readiness Evidence

Date: 2026-07-03

## Status

Implemented as a main CMake/test graph readiness slice for G11
`dflowfm_river_steady`.

G11 remains `status: pending` and `ci_gate: false`. This evidence proves the
runtime adapter boundary and fail-closed behavior, not a real D-Flow FM hydraulic
case. A real external D-Flow FM kernel plus replayable `.mdu` case is still
required before G11 can become an executable Golden or active gate.

## Scope

This change adds:

- `SCAU_ENABLE_DFLOWFM_BMI_RUNTIME` root CMake option;
- conditional `scau::coupling_river` source
  `libs/coupling/river/src/dflowfm_adapter/dflowfm_engine.cpp`;
- public boundary header
  `libs/coupling/river/include/coupling/river/dflowfm_engine.hpp`;
- unit coverage in `tests/unit/coupling/test_coupling_dflowfm_engine.cpp`;
- a test-local fake BMI shared library
  `tests/unit/coupling/fake_dflowfm_bmi.cpp`;
- updated D-Flow FM spike evidence under `spikes/dflowfm/evidence/`.

`DFlowFMEngine` remains a lifecycle/state-IO adapter only. It does not own
`Q_limit`, `V_limit`, deficit, rollback, replay, or arbitration semantics.
Those remain owned by CouplingLib.

## Boundary Checks

The adapter keeps the D-Flow FM BMI firewall:

- no D-Flow FM header or third-party type appears in the public SCAU-UFM API;
- BMI symbols are resolved at runtime from `SCAU_DFLOWFM_LIBRARY` or the platform
  default (`dflowfm.dll` / `libdflowfm.so`);
- missing libraries and missing symbols fail closed through `DFlowFMEngineError`;
- the BMI 1.0 process-global API is guarded so only one initialized
  `DFlowFMEngine` can own the runtime state at a time;
- `get_var` pointers are treated as engine-owned storage;
- writes intentionally support only scalar/location-0 `set_var` until real
  variable shape inventory and `set_var_slice` evidence are recorded.

## Local Validation

Validated from `H:/githubcode/SCAU-UFM/.worktrees/g10b-replay-drift-audit` on the
`windows-msvc` build tree:

```bash
cmake --build H:/githubcode/SCAU-UFM/.worktrees/g10b-replay-drift-audit/build/windows-msvc --config Debug --target test_coupling_dflowfm_engine
ctest --test-dir H:/githubcode/SCAU-UFM/.worktrees/g10b-replay-drift-audit/build/windows-msvc -C Debug -R "^test_coupling_dflowfm_engine$" --output-on-failure
python H:/githubcode/SCAU-UFM/.worktrees/g10b-replay-drift-audit/tests/golden/suite_manifest/check_manifest.py
ctest --test-dir H:/githubcode/SCAU-UFM/.worktrees/g10b-replay-drift-audit/build/windows-msvc -C Debug -L golden --output-on-failure
ctest --test-dir H:/githubcode/SCAU-UFM/.worktrees/g10b-replay-drift-audit/build/windows-msvc -C Debug -R "test_(swmm_single_pipe_surcharge|snapshot_replay_mass_deficit)$" --output-on-failure
```

Results:

- `test_coupling_dflowfm_engine` build: PASS;
- `test_coupling_dflowfm_engine`: PASS;
- manifest checker: PASS;
- active `golden` label set: PASS (9/9 in this worktree, including CVC candidate labels that also carry `golden`-adjacent labels in the local CTest graph);
- G8/G10 targeted tests: PASS (`test_swmm_single_pipe_surcharge`,
  `test_snapshot_replay_mass_deficit`).

## G11 Implication

This is the correct next slice after G8 real SWMM gate promotion: it hardens the
D-Flow FM runtime boundary without claiming a real hydraulic case.

G11 promotion remains blocked on:

1. a real D-Flow FM build made available as `SCAU_DFLOWFM_LIBRARY` or platform
   default runtime library;
2. a replayable `.mdu` case under `spikes/dflowfm/cases/`;
3. a 100-step spike run recording lifecycle, time advance, variable inventory,
   state read/write, units, pointer lifetime, and error behavior;
4. a Golden candidate test that consumes that real runtime evidence without
   moving `Q_limit`, deficit, replay, or arbitration into the river adapter.
