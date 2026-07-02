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

The executable Golden remains mock-boundary coverage, but it now has two real-SWMM backing layers: standalone spike evidence in `spikes/swmm/evidence/g8_real_swmm_inp_evidence.md`, and main-graph runtime evidence through the real `SwmmEngine` test path. The standalone authored `manhole_overflow.inp` and the main-graph copied `swmm_manhole_overflow.inp` both produce real SWMM `NODE_OVERFLOW > 0` at `J1`. This still does not promote G8 to `ci_gate:true`; that decision remains separate from proving the runtime path exists.

## Validation

Validated on 2026-07-01 from `H:/githubcode/SCAU-UFM/.worktrees/g10b-replay-drift-audit`:

- `cmake --preset windows-msvc` — PASS
- `cmake --build build/windows-msvc --config Debug --target test_swmm_single_pipe_surcharge` — PASS
- `ctest --test-dir build/windows-msvc -C Debug -R "^test_swmm_single_pipe_surcharge$" --output-on-failure` — PASS
- `python tests/golden/suite_manifest/check_manifest.py` — PASS
- `ctest --test-dir build/windows-msvc -C Debug -L candidate_non_gating --output-on-failure` — PASS (`test_swmm_single_pipe_surcharge`, `test_snapshot_replay_mass_deficit`)
- Standalone `spikes/swmm` host against authored `.inp` cases — PASS locally; `manhole_overflow.inp` reports `SWMM version = 52004`, `J1` head cap near `0.450000`, and positive `NODE_OVERFLOW` from step 7 (`0.392762`, stabilizing near `0.384070`).
