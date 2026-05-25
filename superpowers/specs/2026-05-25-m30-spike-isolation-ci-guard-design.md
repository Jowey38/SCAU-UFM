# M30 Spike Isolation CI Guard Design

**Date:** 2026-05-25

## Goal

Promote the spike-spec §2 isolation invariant and the §3.3 top-level vendor restriction from manual M28 / M29 verification into a machine-executed CI gate. Any future commit that couples `spikes/` to the main repo, adds spike to the main CMake graph, or sneaks a top-level vendor directory back in must fail CI before the change can merge.

## Normative Anchors

- `superpowers/specs/2026-05-08-third-party-spike-design.md` §2:
  > "spikes/ 不得与 libs/ extern/ third_party/ apps/ 共享 CMake target；spike 自带独立构建系统，避免污染主仓库"
- `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` §3.3:
  top-level vendor placement restricted to `extern/swmm5/`, `extern/dflowfm/`, `extern/dflowfm-bmi/`.
- `superpowers/specs/2026-04-14-scau-ufm-stability-reliability-protocol.md`:
  evidence must be CI-backed; a spec-only invariant without CI enforcement is insufficient for release-gate confidence.

M28 closed the vendor-pollution risk via `.gitignore` (manual verification). M29 added spike skeletons (manual `grep` verification of isolation). M30 turns both into a single CI job that runs on every push and PR.

## Invariants Enforced

The CI job runs four checks. Each must pass; any failure marks the job red.

### Invariant 1: Spike must not reference main repo symbols

```bash
! grep -rnE "scau::coupling|scau::surface|scau::mesh|scau::stcf" spikes/
! grep -rn "libs/coupling" spikes/
! grep -rn "libs/surface2d" spikes/
```

Rationale: per spike-spec §2 second bullet, "`spikes/<engine>/host/` 中代码可直接 include 第三方头文件，**不**走主仓库 §7 接口". A spike that imports `scau::coupling::core` is no longer a spike.

### Invariant 2: Main CMake graph must not consume spike targets

```bash
! grep -nE "add_subdirectory\s*\(\s*spikes" CMakeLists.txt
! grep -rnE "add_subdirectory\s*\([^)]*spikes" libs/ apps/ tests/ cmake/ 2>/dev/null
```

Rationale: per spike-spec §2, spike CMake must remain standalone. The grep on `CMakeLists.txt` catches top-level inclusion; the recursive grep on `libs/`, `apps/`, `tests/`, `cmake/` catches sub-CMakeLists trying to reach into `../spikes`.

### Invariant 3: Spike CMakeLists must not link main repo targets

```bash
! grep -rnE "scau::|GTest::" spikes/*/CMakeLists.txt
```

Rationale: `scau::warnings`, `scau::coupling_core`, `GTest::gtest_main` etc. belong to the main build graph; spike linkage to them re-introduces the coupling that §2 forbids.

Note: M29's spike CMakeLists currently link only `${SWMM_SOLVER_LIB}` / `${DFLOWFM_LIB}` (cache-resolved external paths). The grep must return zero matches.

### Invariant 4: No top-level vendor source under git tracking

```bash
! git ls-files | grep -E "^(Delft3D-main|Stormwater-Management-Model-develop)/"
```

Rationale: M28 added these to `.gitignore`, but a future `git add -f` could bypass it. This check verifies no file under the two forbidden top-level vendor paths has ever been committed.

## CI Job Shape

A new job `spike-isolation-check` is added to `.github/workflows/ci.yml`. It:

- Runs on `ubuntu-22.04` only (single OS sufficient; grep is portable).
- Does **not** invoke vcpkg, CMake, or any compiler; expected runtime < 30 seconds.
- Does **not** depend on `build-and-test`; runs in parallel.
- Uses `bash` with `set -euo pipefail`; each invariant either passes silently or prints the offending matches and exits non-zero.
- Annotates failures with the specific invariant violated, so the failure surface in GitHub UI is informative.

The job is added as a sibling to `build-and-test`, not a dependency. This means:

- A failure in `spike-isolation-check` does not stop `build-and-test` from running (parallel).
- Both must pass for the overall CI check to be green (default behaviour of GitHub Actions matrix on push).

## Tests / Verification

- **Positive case (M30 itself)**: on the M30 commit, all four invariants must pass on master. The local simulation in M30 plan Task 1 verifies this before commit.
- **Negative case (locally only)**: M30 plan Task 2 temporarily injects each kind of violation in turn, verifies the script catches it, then undoes the change. No violation is committed.

## Boundaries

- No change to spike source or `libs/coupling/` / `apps/` / `tests/` source.
- No new compile-time check (grep only; no CMake helper).
- No license review or vendor placement decision (M31+).
- No change to the existing `build-and-test` matrix; the new job is additive.
- No introduction of pre-commit hooks (CI-only enforcement; pre-commit could be added later but is out of scope here).
- No change to `.gitignore` (M28 guard stays).
- No reformatting of the existing `ci.yml`; only additive job definition appended.
