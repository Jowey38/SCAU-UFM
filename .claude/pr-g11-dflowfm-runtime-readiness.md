## Summary

- Add optional `SCAU_ENABLE_DFLOWFM_BMI_RUNTIME` support for a runtime-loaded D-Flow FM BMI adapter.
- Keep the adapter as lifecycle/state IO only: no `Q_limit`, deficit, rollback, replay, or arbitration semantics move into the river boundary.
- Add fail-closed unit coverage plus a test-local fake BMI shared library to exercise positive-path symbol loading, initialize/update/finalize, variable inventory, state read/write, and single-open-project guard.
- Record G11 readiness evidence and keep `G11 dflowfm_river_steady` pending/non-gating until real external D-Flow FM `.mdu` runtime evidence exists.

## Validation

- `cmake --build H:/githubcode/SCAU-UFM/.worktrees/g10b-replay-drift-audit/build/windows-msvc --config Debug --target test_coupling_dflowfm_engine`
- `ctest --test-dir H:/githubcode/SCAU-UFM/.worktrees/g10b-replay-drift-audit/build/windows-msvc -C Debug -R "^test_coupling_dflowfm_engine$" --output-on-failure`
- `python H:/githubcode/SCAU-UFM/.worktrees/g10b-replay-drift-audit/tests/golden/suite_manifest/check_manifest.py`
- `ctest --test-dir H:/githubcode/SCAU-UFM/.worktrees/g10b-replay-drift-audit/build/windows-msvc -C Debug -L golden --output-on-failure`
- `ctest --test-dir H:/githubcode/SCAU-UFM/.worktrees/g10b-replay-drift-audit/build/windows-msvc -C Debug -R "test_(swmm_single_pipe_surcharge|snapshot_replay_mass_deficit)$" --output-on-failure`

## Remaining Blocker

G11 is not promoted to an executable Golden or active gate in this PR. Promotion still requires a real D-Flow FM build, a replayable `.mdu` case, and a recorded 100-step runtime spike.

🤖 Generated with [Claude Code](https://claude.com/claude-code)
