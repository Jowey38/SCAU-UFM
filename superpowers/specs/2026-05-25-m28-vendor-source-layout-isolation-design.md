# M28 Vendor Source Layout Isolation Design

**Date:** 2026-05-25

## Goal

Erect a `.gitignore` barrier that prevents two top-level vendor source trees that recently appeared in the working directory from being committed to `master`, and record an evidence-backed design for their future canonical placement. M28 only installs the guard; it does **not** physically move, modify, or delete the existing local directories.

## Observed Violations

`git status --short` after the M27 push showed two untracked top-level directories that violate the project layout:

| Path | File count | License footprint | Concern |
|---|---|---|---|
| `Delft3D-main/` | 30,720 files | AGPL-3.0 / GPL-3.0 / LGPL-2.1 / BSD-2 / BSD-3 multi-license | Mass third-party source under copyleft licenses; embedded `src/third_party_open/`; D-Flow FM upstream snapshot |
| `Stormwater-Management-Model-develop/` | 112 files | Public-domain US-EPA SWMM5 with own `CMakeLists.txt`, `extern/`, `tests/` | Upstream SWMM5 snapshot with its own build system that would conflict with main repo's CMake graph |

Neither path is in the canonical layout. Both are sitting at the top of `H:/githubcode/SCAU-UFM/` where any future `git add -A` from the user, a tool, or an editor would silently scoop in tens of thousands of vendor files.

## Normative Anchors

- `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` §3.3: top-level vendor placement is restricted to `extern/swmm5/` (SWMM 5.2.x) and `extern/dflowfm/` / `extern/dflowfm-bmi/` (D-Flow FM body + BMI bridge). No other top-level vendor paths are permitted.
- `superpowers/specs/2026-05-08-third-party-spike-design.md` §2: "spikes/ 不得与 libs/ extern/ third_party/ apps/ 共享 CMake target；spike 自带独立构建系统，避免污染主仓库". Vendor sources consumed by a spike host live under `spikes/<engine>/`, not at the repo root.
- `superpowers/specs/project-layout-design.md`: defines the target engineering layout; top-level only contains `libs/`, `apps/`, `extern/`, `third_party/`, `tests/`, `tools/`, `configs/`, `docs/`, `superpowers/`, `cmake/`, `scripts/`, `spikes/`, and build / config metadata. The two observed paths match none of these slots.
- `superpowers/INDEX.md`: the third-party spike spec has conditional-authority status until exit criteria are met; until then, `libs/coupling/drainage/` and `libs/coupling/river/` may not enter formal implementation, and any vendor sources must remain isolated from the main CMake graph.

## Scope

M28 does:

1. Add three lines to `.gitignore` that explicitly ignore the two top-level vendor paths.
2. Verify `git status --short` no longer lists them after the ignore lands.
3. Verify CTest still passes (`31/31`), proving the ignore change does not interfere with the build.
4. Write design + plan + evidence documents (this file, the plan, and an evidence record).

M28 does **not**:

- Delete, move, rename, or modify the contents of `Delft3D-main/` or `Stormwater-Management-Model-develop/`. The user's local copy stays exactly where it is.
- Decide which of the three legitimate placements (see below) is correct. That decision belongs to a follow-up milestone that first reads the spike spec exit criteria in detail.
- Introduce any `extern/` or `spikes/` directory structure, CMake target, license manifest, or third-party policy file.
- Touch `libs/coupling/`, `cmake/`, or any test target.

## Three Legitimate Future Placements

A follow-up milestone (≥ M29) must choose **exactly one** placement per vendor source, document it, and migrate. M28 only enumerates the options:

| Option | Path under main repo | When appropriate |
|---|---|---|
| Spike vendor mirror | `spikes/swmm/vendor/swmm5/`, `spikes/dflowfm/vendor/delft3d/` | While third-party spike (§7) is in progress; isolated CMake; vendor files entered into git only if redistribution license permits |
| `extern/` vendor snapshot | `extern/swmm5/`, `extern/dflowfm/` (+ `extern/dflowfm-bmi/`) | After spike exit; canonical embedded location per §3.3; requires `cmake/third_party/` discovery, version manifest, license review |
| External git submodule | `extern/swmm5/` and `extern/dflowfm/` as submodules referencing upstream | If license forbids redistribution in this repo or upstream churn is too fast to manage as a snapshot |

The AGPL-3.0 / GPL-3.0 content inside `Delft3D-main/` and its mixed-license `src/third_party_open/` make the D-Flow FM placement decision **non-trivial**; M29 must include explicit legal review notes per §3.3 and §3.4.

## `.gitignore` Patch

Append the following block to `.gitignore`:

```gitignore

# Top-level vendor sources must never enter git history.
# Canonical layout per §3.3: extern/swmm5/, extern/dflowfm/, extern/dflowfm-bmi/.
# Spike vendor mirror per third-party-spike-design §2: spikes/<engine>/vendor/.
# See superpowers/specs/2026-05-25-m28-vendor-source-layout-isolation-design.md.
/Delft3D-main/
/Stormwater-Management-Model-develop/
```

The leading `/` anchors each pattern to the repo root so a future legitimate `spikes/swmm/vendor/Stormwater-Management-Model-develop/` (if ever needed) is not accidentally ignored.

## Tests / Verification

- `git status --short` before patch: lists both directories as untracked.
- `git status --short` after patch: does not list either directory.
- `git check-ignore -v Delft3D-main Stormwater-Management-Model-develop`: each path resolves to the new `.gitignore` rule.
- `ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: still 31/31 (proves the guard has no side effect on the build graph).

## Boundaries

- No physical migration in M28.
- No license review in M28 (deferred to M29 placement decision).
- No `extern/` / `spikes/` / `cmake/third_party/` scaffolding in M28.
- No change to `libs/`, `apps/`, `tests/`, `cmake/`, or any CMake graph.
- No statement on whether the two directories should ultimately be committed, submodule-referenced, or kept purely local — that is the M29 decision.
