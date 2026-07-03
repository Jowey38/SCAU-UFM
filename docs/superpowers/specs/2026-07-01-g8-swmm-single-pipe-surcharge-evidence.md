# G8 swmm_single_pipe_surcharge Evidence

## Status

Implemented and promoted to an active Golden gate (`ci_gate:true`).

## Scope

G8 currently validates the executable Phase-1 boundary available in this repository:

- real SWMM surcharge/overflow generation through `SwmmEngine` and the authored `swmm_manhole_overflow.inp` case;
- CouplingLib hard gate semantics using `V_limit = 0.9 * phi_t * h * A` and `Q_limit = V_limit / dt_sub`;
- a real SWMM `NODE_OVERFLOW` request from `J1` that exceeds the cell `Q_limit` and is clamped;
- unmet volume recorded in `mass_deficit_account` after replay;
- follow-up zero-request step repays outstanding deficit without exceeding the recomputed `Q_limit`;
- SWMM adapter acceptance writes `q_granted + q_repay` into node lateral inflow, verified after a follow-up routing step.

G8 now has three aligned evidence layers: the executable Golden itself runs on real `SwmmEngine`, standalone spike evidence exists in `spikes/swmm/evidence/g8_real_swmm_inp_evidence.md`, and the main-graph runtime evidence exists through `test_coupling_swmm_engine`. This still does not promote G8 to `ci_gate:true`; that decision remains separate from proving the runtime path exists.

## Validation

Validated from `H:/githubcode/SCAU-UFM/.worktrees/g10b-replay-drift-audit`:

- 2026-07-01 mock-boundary candidate path — PASS
- 2026-07-02 real-runtime candidate path with `SwmmEngine` + `swmm_manhole_overflow.inp` — PASS
- `cmake --preset windows-msvc` — PASS
- `cmake --build build/windows-msvc --config Debug --target test_swmm_single_pipe_surcharge` — PASS
- `ctest --test-dir build/windows-msvc -C Debug -R "^test_swmm_single_pipe_surcharge$" --output-on-failure` — PASS
- `python tests/golden/suite_manifest/check_manifest.py` — PASS
- `ctest --test-dir build/windows-msvc -C Debug -L candidate_non_gating --output-on-failure` — PASS (`test_swmm_single_pipe_surcharge`, `test_snapshot_replay_mass_deficit`)
- Standalone `spikes/swmm` host against authored `.inp` cases — PASS locally; `manhole_overflow.inp` reports `SWMM version = 52004`, `J1` head cap near `0.450000`, and positive `NODE_OVERFLOW` from step 7 (`0.392762`, stabilizing near `0.384070`).
