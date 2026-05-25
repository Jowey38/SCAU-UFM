# M28 Vendor Source Layout Isolation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to walk through Task 1 → Task 3 in order.

**Goal:** Install a `.gitignore` guard against the two top-level vendor directories that recently appeared (`Delft3D-main/`, `Stormwater-Management-Model-develop/`), and record evidence proving the guard works and the build is unaffected. No physical migration, no license review, no CMake change.

**Tech Stack:** git, CMake, GoogleTest (only as smoke regression).

---

## Task 1: Pre-patch Inventory

**Files:** none modified.

- [ ] **Step 1: Record current top-level state**

```bash
git status --short
```

Expected output includes (at minimum):

```
?? Delft3D-main/
?? Stormwater-Management-Model-develop/
```

Capture this in the evidence document later.

- [ ] **Step 2: Confirm neither directory is currently ignored**

```bash
git check-ignore -v Delft3D-main Stormwater-Management-Model-develop
```

Expected: exit code 1 (neither is ignored yet); no output lines.

- [ ] **Step 3: Confirm file counts match design**

```bash
find H:/githubcode/SCAU-UFM/Delft3D-main -type f | wc -l
find H:/githubcode/SCAU-UFM/Stormwater-Management-Model-develop -type f | wc -l
```

Expected: 30720 and 112. If counts have drifted significantly, note the new numbers in evidence.

---

## Task 2: Install `.gitignore` Guard

**Files:**
- Modify: `.gitignore`

- [ ] **Step 1: Append the guard block**

Append to `.gitignore` (preserving the trailing newline of the existing file):

```gitignore

# Top-level vendor sources must never enter git history.
# Canonical layout per §3.3: extern/swmm5/, extern/dflowfm/, extern/dflowfm-bmi/.
# Spike vendor mirror per third-party-spike-design §2: spikes/<engine>/vendor/.
# See superpowers/specs/2026-05-25-m28-vendor-source-layout-isolation-design.md.
/Delft3D-main/
/Stormwater-Management-Model-develop/
```

- [ ] **Step 2: Verify the guard removes the entries from `git status`**

```bash
git status --short
```

Expected: neither `Delft3D-main/` nor `Stormwater-Management-Model-develop/` appears. The transcript `2026-05-20-163432-...txt` may still be listed as untracked; that is unrelated.

- [ ] **Step 3: Verify each path is ignored by the new rule**

```bash
git check-ignore -v Delft3D-main Stormwater-Management-Model-develop
```

Expected: each path resolves to the `/Delft3D-main/` or `/Stormwater-Management-Model-develop/` rule from `.gitignore`.

- [ ] **Step 4: Run full CTest to confirm the build graph is unaffected**

```bash
PATH="/d/Program Files/CMake/bin:$PATH" ctest --test-dir build/windows-msvc -C Debug --output-on-failure
```

Expected: 31/31 PASS (M27 baseline). If the count drops or any test fails, the `.gitignore` change is not actually inert and Task 2 must be reverted before evidence is written.

---

## Task 3: Evidence

**Files:**
- Create: `superpowers/specs/2026-05-25-plan-m28-vendor-source-layout-isolation-evidence.md`

- [ ] **Step 1: Write the evidence document**

Create `superpowers/specs/2026-05-25-plan-m28-vendor-source-layout-isolation-evidence.md` with the structure:

```markdown
# M28 Vendor Source Layout Isolation Evidence

**Date:** 2026-05-25
**Design:** `superpowers/specs/2026-05-25-m28-vendor-source-layout-isolation-design.md`
**Plan:** `superpowers/plans/2026-05-25-plan-m28-vendor-source-layout-isolation.md`

## Scope

M28 installs a `.gitignore` guard against the two top-level vendor directories (`Delft3D-main/`, `Stormwater-Management-Model-develop/`) that violate §3.3 and the spike-spec §2 isolation rule. No physical migration, no license review, no CMake change.

## Pre-patch State

- `git status --short` listed `?? Delft3D-main/` and `?? Stormwater-Management-Model-develop/` as untracked.
- `git check-ignore -v Delft3D-main Stormwater-Management-Model-develop`: exit 1, no rule covered them.
- File counts: `Delft3D-main/` = 30720 files; `Stormwater-Management-Model-develop/` = 112 files.

## `.gitignore` Patch

Six new lines appended (block header + 2 anchored rules + 3 comment lines documenting canonical layout).

## Post-patch Verification

- `git status --short`: neither directory listed.
- `git check-ignore -v Delft3D-main`: matched rule `/Delft3D-main/` in `.gitignore`.
- `git check-ignore -v Stormwater-Management-Model-develop`: matched rule `/Stormwater-Management-Model-develop/` in `.gitignore`.
- `ctest --test-dir build/windows-msvc -C Debug --output-on-failure`: PASS, 31/31.

## §3.3 / Spike §2 Alignment

- §3.3 mandates `extern/swmm5/`, `extern/dflowfm/`, `extern/dflowfm-bmi/` as the only top-level vendor placements. Neither observed directory matches.
- Spike §2 mandates that spike vendor consumers live under `spikes/<engine>/`, isolated from the main CMake graph. The current top-level placement violates this isolation rule.
- M28 surfaces both violations without making the placement decision; the placement decision is deferred to ≥ M29 with explicit legal review for the AGPL/GPL content inside `Delft3D-main/`.

## Boundaries

- M28 does not move, modify, or delete the contents of either directory.
- M28 does not introduce `extern/` or `spikes/` scaffolding.
- M28 does not touch `libs/`, `apps/`, `tests/`, `cmake/`, or any CMake target.
- M28 does not perform license review.
- M28 does not decide between the three placement options enumerated in the design; that decision is M29.
```

Fill in any observed numbers that differ from the design (e.g. updated file counts).

- [ ] **Step 2: Inspect status and diff**

```bash
git status --short
git diff -- .gitignore
```

Expected: `.gitignore` is modified; three new untracked files appear (design, plan, evidence); transcript stays untracked; the two vendor directories no longer appear.

---

## Boundaries

- `.gitignore` patch only; no `extern/`, no `spikes/`, no CMake, no source edits.
- No license review.
- No movement of vendor directories.
- No discussion of which legitimate placement is correct beyond enumerating the three options in the design.
