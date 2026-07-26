# Real D-Flow FM CI gate decision record

Date: 2026-07-25

Decision scope: promotion path for the five real-runtime goldens
(G11 `dflowfm_river_steady`, G16 `dual_engine_shared_cell_real_both`,
G17 `tri_coupling_real_minimal`, G18 `dflowfm_lateral_response`, and
`dflowfm_checkpoint_reload`) from `ci_gate:false` to enforced CI gates.

## Adopted decision (three stages)

1. Immediate (landed in this change): the reproducible one-command
   Phase Gateway `tools/dflowfm/run_real_goldens.sh` is the release-side
   real-runtime gate. It rebuilds nothing itself; it sequences the five
   goldens against the authored `single_reach_1d` case, regenerates the
   deterministic 600 s native checkpoint before the reload golden, and
   fails on any non-pass. Verified locally: 5/5.
2. Mid-term (infrastructure action, one flip): register a governed
   self-hosted runner labeled `[self-hosted, windows, dflowfm]` with the
   licensed runtime preinstalled, then set the repository variable
   `SCAU_DFLOWFM_RUNNER=enabled`. The pre-wired `real-dflowfm-golden` CI
   job activates automatically; until then it is skipped and never blocks.
3. Long-term (optional evolution): move the runtime into a private
   artifact/container registry pulled with credentials. Not started; it is
   an independent infrastructure/licensing project and does not block the
   mid-term gate.

## Runner prerequisites (mid-term)

- Windows runner with VS2022 and CMake installed; labels
  `[self-hosted, windows, dflowfm]`.
- Repository variables (no machine env or service restart needed):
  - `SCAU_DFLOWFM_RUNNER=enabled` activates the job;
  - `SCAU_DFLOWFM_LIBRARY` -> real `dflowfm.dll` on the runner, with its
    dependent runtime DLLs in the same directory;
  - `SCAU_DFLOWFM_VCPKG_ROOT` -> a bootstrapped vcpkg checkout on the runner.
- Job run steps use bash (Git Bash), so restrictive PowerShell execution
  policies on the runner cannot break the gate.
- Short workspace path (MSVC MAX_PATH; see the project short-build-root
  convention).
- Private/internal PR execution only; do not expose the licensed runtime
  through a public runner.

2026-07-25 activation note: the first live run failed before configure for
two runner-environment reasons (PowerShell script execution disabled, and the
workflow-level `VCPKG_ROOT` override shadowing the machine layout without a
bootstrap step in this job). Both are fixed by the bash default shell and the
repo-variable wiring above.

## Promotion executed (2026-07-26)

The runner `SCAU-DFLOWFM-SERVER-01` is registered and the variable is enabled.
After the runner-portability fixes, `real-dflowfm-golden` ran the full gateway
live and `master` recorded three consecutive fully green runs including the
real gate: `30182833482`, `30183147705`, `30183428672`. Per the criteria
below, G11/G16/G17/G18 are promoted to `ci_gate:true` (manifest, checker
REQUIRED tuples, and `LABELS golden`). Without the runtime env the tests
remain explicit skips; the enforcing execution is the self-hosted gateway job.

## ci_gate promotion criteria

Per the stability protocol, GoldenSuite entries may serve as merge/release
evidence only when represented in CI. Therefore:

- keep `ci_gate:false` for the five real goldens until the
  `real-dflowfm-golden` job is enabled and green on `master`;
- after the job is green on master (recommended: three consecutive runs),
  flip the five manifest entries and `check_manifest.py` REQUIRED tuples to
  `ci_gate:true` in a dedicated PR;
- never substitute mocks inside these goldens to make CI pass without the
  runtime; absence of the runtime must remain an explicit skip locally and
  an explicit job-level requirement in CI.

## Operational note discovered while landing the gateway

This real D-Flow build silently skips map/his/rst output (and truncates
cache file naming) when initialized through a long absolute MDU path, while
computation and state reads keep working. The gateway therefore runs every
golden with cwd at the case directory and a relative MDU name; the
checkpoint-reload golden depends on this to regenerate its 600 s checkpoint.
