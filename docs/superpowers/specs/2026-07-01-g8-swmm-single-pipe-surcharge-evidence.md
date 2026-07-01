# G8 swmm_single_pipe_surcharge Evidence

## Status

Implemented as a non-gating Golden candidate (`ci_gate:false`).

## Scope

G8 currently validates the executable Phase-1 boundary available in this repository:

- SWMM-side deterministic surcharge state through `MockSwmmEngine`;
- CouplingLib hard gate semantics using `V_limit = 0.9 * phi_t * h * A` and `Q_limit = V_limit / dt_sub`;
- surface-to-SWMM drain requests clamped at `Q_limit`;
- unmet volume recorded in `mass_deficit_account` after replay;
- follow-up zero-request step repays outstanding deficit without exceeding the recomputed `Q_limit`;
- SWMM adapter acceptance writes `q_granted + q_repay` into node lateral inflow.

This does not claim real SWMM transient `.inp` coverage; `spikes/swmm/cases/` currently contains only README placeholders. Real single-pipe/manhole-overflow input coverage remains a follow-up before promoting G8 to `ci_gate:true`.

## Validation

Validated on 2026-07-01 from `H:/githubcode/SCAU-UFM/.worktrees/g10b-replay-drift-audit`:

- `cmake --preset windows-msvc` — PASS
- `cmake --build build/windows-msvc --config Debug --target test_swmm_single_pipe_surcharge` — PASS
- `ctest --test-dir build/windows-msvc -C Debug -R "^test_swmm_single_pipe_surcharge$" --output-on-failure` — PASS
- `python tests/golden/suite_manifest/check_manifest.py` — PASS
- `ctest --test-dir build/windows-msvc -C Debug -L candidate_non_gating --output-on-failure` — PASS (`test_swmm_single_pipe_surcharge`, `test_snapshot_replay_mass_deficit`)
