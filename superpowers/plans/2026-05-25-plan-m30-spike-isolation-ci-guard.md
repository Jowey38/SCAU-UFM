# M30 Spike Isolation CI Guard Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to walk Task 1 → Task 3 in order.

**Goal:** Add a new `spike-isolation-check` job to `.github/workflows/ci.yml` that enforces the four spike-spec §2 / §3.3 invariants defined in the M30 design. Pure-additive CI change; no source code, CMake, or `.gitignore` modification.

**Tech Stack:** GitHub Actions, bash, grep, git ls-files.

---

## Task 1: Locally Simulate The Script And Append CI Job

**Files:**
- Modify `.github/workflows/ci.yml`

- [ ] **Step 1: Locally dry-run all four invariants on master**

Run the following block from the repo root:

```bash
set -e

echo "--- Invariant 1: spike must not reference main repo symbols"
! grep -rnE "scau::coupling|scau::surface|scau::mesh|scau::stcf" spikes/ || { echo "FAIL inv1"; exit 1; }
! grep -rn "libs/coupling" spikes/ || { echo "FAIL inv1"; exit 1; }
! grep -rn "libs/surface2d" spikes/ || { echo "FAIL inv1"; exit 1; }

echo "--- Invariant 2: main CMake graph must not consume spike targets"
! grep -nE "add_subdirectory\s*\(\s*spikes" CMakeLists.txt || { echo "FAIL inv2 root"; exit 1; }
if [ -d libs ]   ; then ! grep -rnE "add_subdirectory\s*\([^)]*spikes" libs/   || { echo "FAIL inv2 libs"; exit 1; }; fi
if [ -d apps ]   ; then ! grep -rnE "add_subdirectory\s*\([^)]*spikes" apps/   || { echo "FAIL inv2 apps"; exit 1; }; fi
if [ -d tests ]  ; then ! grep -rnE "add_subdirectory\s*\([^)]*spikes" tests/  || { echo "FAIL inv2 tests"; exit 1; }; fi
if [ -d cmake ]  ; then ! grep -rnE "add_subdirectory\s*\([^)]*spikes" cmake/  || { echo "FAIL inv2 cmake"; exit 1; }; fi

echo "--- Invariant 3: spike CMakeLists must not link main repo targets"
! grep -rnE "scau::|GTest::" spikes/*/CMakeLists.txt || { echo "FAIL inv3"; exit 1; }

echo "--- Invariant 4: no top-level vendor source under git tracking"
if git ls-files | grep -E "^(Delft3D-main|Stormwater-Management-Model-develop)/" ; then
    echo "FAIL inv4"; exit 1
fi

echo "OK all four spike-isolation invariants pass"
```

Expected: prints "OK all four spike-isolation invariants pass" and exits 0.

If any invariant fails on master before M30 lands, STOP. M28 / M29 evidence was inaccurate; investigate before adding the CI gate.

- [ ] **Step 2: Append the new CI job**

Append the following block to `.github/workflows/ci.yml` (after the existing `build-and-test` job, preserving its indentation level so it becomes a sibling top-level job):

```yaml

  spike-isolation-check:
    name: spike-isolation-check
    runs-on: ubuntu-22.04
    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Enforce spike-spec §2 / §3.3 isolation invariants
        shell: bash
        run: |
          set -euo pipefail

          fail() {
            echo "::error::M30 spike-isolation invariant violated: $1"
            exit 1
          }

          echo "--- Invariant 1: spike must not reference main repo symbols"
          if grep -rnE "scau::coupling|scau::surface|scau::mesh|scau::stcf" spikes/ ; then
              fail "spike references scau:: namespace (spike-spec §2)"
          fi
          if grep -rn "libs/coupling" spikes/ ; then
              fail "spike references libs/coupling path (spike-spec §2)"
          fi
          if grep -rn "libs/surface2d" spikes/ ; then
              fail "spike references libs/surface2d path (spike-spec §2)"
          fi

          echo "--- Invariant 2: main CMake graph must not consume spike targets"
          if grep -nE "add_subdirectory\s*\(\s*spikes" CMakeLists.txt ; then
              fail "root CMakeLists.txt calls add_subdirectory(spikes...) (spike-spec §2)"
          fi
          for dir in libs apps tests cmake ; do
              if [ -d "$dir" ] && grep -rnE "add_subdirectory\s*\([^)]*spikes" "$dir" ; then
                  fail "$dir/ contains add_subdirectory targeting spikes/ (spike-spec §2)"
              fi
          done

          echo "--- Invariant 3: spike CMakeLists must not link main repo targets"
          if grep -rnE "scau::|GTest::" spikes/*/CMakeLists.txt ; then
              fail "spike CMakeLists links a main-repo CMake target (spike-spec §2)"
          fi

          echo "--- Invariant 4: no top-level vendor source under git tracking"
          if git ls-files | grep -E "^(Delft3D-main|Stormwater-Management-Model-develop)/" ; then
              fail "top-level vendor source committed (§3.3)"
          fi

          echo "OK all four spike-isolation invariants pass"
```

The new job is at the same indentation as `build-and-test:`; it must remain a sibling, not a step under `build-and-test`.

- [ ] **Step 3: Re-run the local dry-run**

Re-run the block from Step 1. Expected: still prints "OK all four spike-isolation invariants pass" and exits 0. The `.yml` change does not affect local behaviour, but this proves no merge artifact slipped in.

---

## Task 2: Locally Simulate The Negative Path

This task verifies the script actually catches violations. **No violation is committed.**

- [ ] **Step 1: Verify Invariant 1 negative**

Append a temporary line to `spikes/swmm/host/swmm_spike_host.cpp` that imports a main-repo namespace, run the dry-run script, expect failure, then revert.

```bash
echo "// scau::coupling::core::CouplingState dummy;" >> spikes/swmm/host/swmm_spike_host.cpp
# run the inv1 block — expect FAIL inv1
# revert:
git checkout -- spikes/swmm/host/swmm_spike_host.cpp
```

Confirm the working tree is clean afterwards: `git diff -- spikes/swmm/host/swmm_spike_host.cpp` returns empty.

- [ ] **Step 2: Verify Invariant 3 negative**

Append a temporary line to `spikes/swmm/CMakeLists.txt` that links a main-repo target, run inv3, expect failure, then revert.

```bash
echo "# target_link_libraries(swmm_spike_host PRIVATE scau::warnings)" >> spikes/swmm/CMakeLists.txt
# this is a comment; it still matches the grep on "scau::" — that's the point.
# The check is intentionally strict: even commented references in spike CMakeLists fail.
# run inv3 — expect FAIL inv3
git checkout -- spikes/swmm/CMakeLists.txt
```

If you want a less strict version that only catches actual code (not comments), the M30 design can be amended. For M30 the strict version is preferred because spike CMakeLists are tiny and commented references are still a smell.

Confirm: `git diff -- spikes/swmm/CMakeLists.txt` returns empty.

- [ ] **Step 3: Document negative-test results**

Record observations from steps 1 and 2 in the M30 evidence file (Task 3). Do not leave any test artifact in the working tree.

---

## Task 3: Push And Observe New CI Job

**Files:**
- Create: `superpowers/specs/2026-05-25-plan-m30-spike-isolation-ci-guard-evidence.md`

- [ ] **Step 1: Confirm working tree is clean before commit**

```bash
git status --short
```

Expected: only `.github/workflows/ci.yml` modified; plus untracked M30 design / plan / evidence files; plus the unchanged transcript.

- [ ] **Step 2: Stage, commit, push**

```bash
git add .github/workflows/ci.yml \
        superpowers/specs/2026-05-25-m30-spike-isolation-ci-guard-design.md \
        superpowers/plans/2026-05-25-plan-m30-spike-isolation-ci-guard.md \
        superpowers/specs/2026-05-25-plan-m30-spike-isolation-ci-guard-evidence.md

git commit -m "$(cat <<'EOF'
ci: enforce spike isolation invariants on every push

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"

git push origin master
```

- [ ] **Step 3: Observe new CI job**

```bash
gh run watch <run-id> --exit-status
gh run view <run-id>
```

Expected jobs: `linux-gcc`, `windows-msvc`, **`spike-isolation-check`**. All three must complete green. `spike-isolation-check` should complete in well under 30 seconds.

- [ ] **Step 4: Write evidence**

Create `superpowers/specs/2026-05-25-plan-m30-spike-isolation-ci-guard-evidence.md` recording:
- Local dry-run output (positive: 4 invariants pass).
- Local negative simulations (each invariant flagged the injected violation).
- CI run id + job durations.
- Confirmation that all three CI jobs went green on the M30 push.

---

## Boundaries

- No change to spike source.
- No change to `libs/coupling/` / `apps/` / `tests/` source.
- No change to existing `build-and-test` matrix.
- No license review.
- No new pre-commit hook (CI-only).
- No `.gitignore` change.
- No new bash dependency beyond grep / git.
