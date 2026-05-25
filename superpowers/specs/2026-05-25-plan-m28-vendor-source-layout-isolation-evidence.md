# M28 Vendor Source Layout Isolation Evidence

**Date:** 2026-05-25
**Design:** `superpowers/specs/2026-05-25-m28-vendor-source-layout-isolation-design.md`
**Plan:** `superpowers/plans/2026-05-25-plan-m28-vendor-source-layout-isolation.md`

## Scope

M28 installs a `.gitignore` guard against the two top-level vendor directories (`Delft3D-main/`, `Stormwater-Management-Model-develop/`) that violate §3.3 and the spike-spec §2 isolation rule. No physical migration, no license review, no CMake change.

## Pre-patch State

- `git status --short` listed both directories as untracked:
  ```
  ?? Delft3D-main/
  ?? Stormwater-Management-Model-develop/
  ```
- `git check-ignore -v Delft3D-main Stormwater-Management-Model-develop`: exit 1; no rule covered either path.
- File counts: `Delft3D-main/` = 30720 files; `Stormwater-Management-Model-develop/` = 112 files.

## `.gitignore` Patch

Six new lines appended after the "Local editor and OS files" block:

```gitignore

# Top-level vendor sources must never enter git history.
# Canonical layout per §3.3: extern/swmm5/, extern/dflowfm/, extern/dflowfm-bmi/.
# Spike vendor mirror per third-party-spike-design §2: spikes/<engine>/vendor/.
# See superpowers/specs/2026-05-25-m28-vendor-source-layout-isolation-design.md.
/Delft3D-main/
/Stormwater-Management-Model-develop/
```

The leading `/` anchors each pattern to the repo root so a hypothetical future legitimate `spikes/<engine>/vendor/<same-name>/` placement is not accidentally ignored.

## Post-patch Verification

- `git status --short`: neither `Delft3D-main/` nor `Stormwater-Management-Model-develop/` appears. Remaining untracked entries are only the M28 design / plan / evidence files and the existing session transcript.
- `git check-ignore -v Delft3D-main`: matched `.gitignore:28:/Delft3D-main/`.
- `git check-ignore -v Stormwater-Management-Model-develop`: matched `.gitignore:29:/Stormwater-Management-Model-develop/`.
- `PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, 31/31 (9.80s). Identical to the M27 baseline, proving the guard has no effect on the CMake build graph.

## §3.3 / Spike §2 Alignment

- §3.3 restricts top-level vendor placement to `extern/swmm5/`, `extern/dflowfm/`, and `extern/dflowfm-bmi/`. Neither observed directory matches any of these slots.
- Spike-spec §2 forbids `spikes/` from sharing CMake targets with `libs/`, `extern/`, `third_party/`, or `apps/`; spike vendor consumers must live under `spikes/<engine>/`, isolated from the main build graph. The current top-level placement violates this isolation rule even for spike use.
- M28 surfaces both violations and removes them from the path of an accidental `git add -A` without making the placement decision; that decision is deferred to ≥ M29 with explicit legal review for the AGPL-3.0 / GPL-3.0 / LGPL-2.1 / BSD-2 / BSD-3 multi-license content inside `Delft3D-main/`.

## Boundaries

- M28 does not move, modify, or delete the contents of either directory; the user's local copy is preserved exactly where it sits.
- M28 does not introduce `extern/` or `spikes/` scaffolding, CMake target, version manifest, or license manifest.
- M28 does not touch `libs/`, `apps/`, `tests/`, `cmake/`, or any CMake target.
- M28 does not perform license review.
- M28 does not decide between the three placement options enumerated in the design; that decision is M29.
