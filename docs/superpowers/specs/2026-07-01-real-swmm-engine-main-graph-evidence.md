# Real SwmmEngine Main-Graph Evidence

Date: 2026-07-01

## Status

Implemented as a minimal main CMake/test graph integration for the real embedded
SWMM 5.2.4 engine.

## Scope

This change adds:

- `SCAU_EMBED_SWMM` root CMake option;
- `cmake/third_party/swmm.cmake` building vendored SWMM C sources as
  `scau::extern_swmm5`;
- vendored SWMM solver snapshot under `extern/swmm5/`;
- third-party governance records under `third_party/manifest/` and
  `third_party/licenses/`;
- `scau::coupling_drainage` conditional real `SwmmEngine` source;
- `tests/unit/coupling/test_coupling_swmm_engine.cpp` in the main CTest graph.

`SwmmEngine` remains a lifecycle/state-IO adapter only. It does not own
`Q_limit`, `V_limit`, deficit, rollback, replay, or arbitration semantics.

## Boundary checks

The adapter keeps the SWMM C API firewall:

- `swmm5.h` is included only by
  `libs/coupling/drainage/src/swmm_adapter/swmm_engine.cpp`;
- public headers expose only project C++ types;
- SWMM process-wide global state is guarded so only one real `SwmmEngine` can be
  open at a time.

## Local validation

Validated from `H:/githubcode/SCAU-UFM/.worktrees/g10b-replay-drift-audit`:

```bash
cmake -S H:/githubcode/SCAU-UFM/.worktrees/g10b-replay-drift-audit --preset windows-msvc
cmake --build H:/githubcode/SCAU-UFM/.worktrees/g10b-replay-drift-audit/build/windows-msvc --config Debug --target test_coupling_swmm_engine
ctest --test-dir H:/githubcode/SCAU-UFM/.worktrees/g10b-replay-drift-audit/build/windows-msvc -C Debug -R "^test_coupling_swmm_engine$" --output-on-failure
python H:/githubcode/SCAU-UFM/.worktrees/g10b-replay-drift-audit/tests/golden/suite_manifest/check_manifest.py
ctest --test-dir H:/githubcode/SCAU-UFM/.worktrees/g10b-replay-drift-audit/build/windows-msvc -C Debug -L candidate_non_gating --output-on-failure
ctest --test-dir H:/githubcode/SCAU-UFM/.worktrees/g10b-replay-drift-audit/build/windows-msvc -C Debug -L golden --output-on-failure
```

Results:

- configure: PASS;
- `test_coupling_swmm_engine` build: PASS;
- `test_coupling_swmm_engine`: PASS;
- manifest checker: PASS;
- `candidate_non_gating`: PASS (`test_swmm_single_pipe_surcharge`,
  `test_snapshot_replay_mass_deficit`);
- `golden`: PASS for active G1-G6.

## G8 implication

This is the first step after standalone spike evidence toward CI-gated real SWMM
coverage. G8 remains `ci_gate:false` until remote Linux/Windows CI proves the
real SWMM main-graph test is deterministic and the GoldenSuite policy is updated
intentionally.
