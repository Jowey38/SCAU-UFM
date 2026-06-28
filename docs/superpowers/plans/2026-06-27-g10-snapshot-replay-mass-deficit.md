# G10 Snapshot Replay Mass Deficit Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Promote GoldenSuite entry `G10 snapshot_replay_mass_deficit` from `pending` to an implemented Phase-1 pure in-memory CouplingLib core Golden.

**Architecture:** Reuse the already-proven CouplingLib core snapshot/rollback/replay and mass-audit semantics from `tests/unit/coupling/test_coupling_core_state.cpp` and `test_coupling_mass_deficit_account.cpp`, then consolidate them into a dedicated Golden under `tests/golden/snapshot_replay_mass_deficit/`. The first landing keeps `ci_gate: false` and proves deterministic replayable-core behavior without introducing real 1D solver transient replay or any file-backed snapshot path.

**Tech Stack:** C++20, CMake/CTest, GoogleTest, Python 3 manifest checker, Windows MSVC preset with vcpkg.

---

## File Structure

- `tests/golden/CMakeLists.txt` — add the new Golden subdirectory.
- `tests/golden/snapshot_replay_mass_deficit/CMakeLists.txt` — build `test_snapshot_replay_mass_deficit` and label it `golden`.
- `tests/golden/snapshot_replay_mass_deficit/test_snapshot_replay_mass_deficit.cpp` — the new G10 Golden narrative; consolidates aggregate deficit, shared endpoint deficit, pending-event fidelity, and system mass audit equivalence.
- `tests/golden/suite_manifest/goldensuite.json` — update G10 from `pending`/`ci_gate:false` to `implemented`/`ci_gate:false`, leaving G1-G12 matrix shape unchanged.
- `superpowers/INDEX.md` — add G10 implementation evidence entry after implementation.
- `docs/superpowers/specs/2026-06-27-g10-snapshot-replay-mass-deficit-evidence.md` — archive implementation evidence after tests pass.

Reference-only inputs to reuse, not to modify unless absolutely necessary:

- `tests/unit/coupling/test_coupling_core_state.cpp`
- `tests/unit/coupling/test_coupling_mass_deficit_account.cpp`
- `libs/coupling/core/include/coupling/core/state.hpp`
- `libs/coupling/core/src/state.cpp`
- `tests/golden/suite_manifest/check_manifest.py`

No new `pipe1d` or external-engine files are part of G10 v1.

---

### Task 1: Add the G10 Golden target skeleton

**Files:**
- Modify: `tests/golden/CMakeLists.txt`
- Create: `tests/golden/snapshot_replay_mass_deficit/CMakeLists.txt`
- Create: `tests/golden/snapshot_replay_mass_deficit/test_snapshot_replay_mass_deficit.cpp`

- [ ] **Step 1: Add the new Golden subdirectory to the root Golden CMakeLists**

In `tests/golden/CMakeLists.txt`, append:

```cmake
add_subdirectory(snapshot_replay_mass_deficit)
```

- [ ] **Step 2: Create the per-test CMake target**

Create `tests/golden/snapshot_replay_mass_deficit/CMakeLists.txt`:

```cmake
add_executable(test_snapshot_replay_mass_deficit test_snapshot_replay_mass_deficit.cpp)
target_link_libraries(test_snapshot_replay_mass_deficit
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_snapshot_replay_mass_deficit COMMAND test_snapshot_replay_mass_deficit)
set_tests_properties(test_snapshot_replay_mass_deficit PROPERTIES LABELS golden)
```

- [ ] **Step 3: Create a compile-only Golden test skeleton**

Create `tests/golden/snapshot_replay_mass_deficit/test_snapshot_replay_mass_deficit.cpp`:

```cpp
#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

TEST(SnapshotReplayMassDeficitGolden, SkeletonBuilds) {
    scau::coupling::core::CouplingState state{{
        {.volume = 0.0, .phi_t = 1.0, .h = 0.0, .area = 1.0},
    }};

    const auto snapshot = state.snapshot();
    EXPECT_EQ(snapshot.cells().size(), 1U);
}
```

- [ ] **Step 4: Run the new target to verify the scaffold builds and passes**

Run:

```bash
cmake --preset windows-msvc
cmake --build --preset windows-msvc --target test_snapshot_replay_mass_deficit
ctest --preset windows-msvc -C Debug -R "^test_snapshot_replay_mass_deficit$" -V
```

Expected: PASS with a single trivial test.

- [ ] **Step 5: Commit**

```bash
git add tests/golden/CMakeLists.txt tests/golden/snapshot_replay_mass_deficit/CMakeLists.txt tests/golden/snapshot_replay_mass_deficit/test_snapshot_replay_mass_deficit.cpp
git commit -m "test(golden): scaffold G10 snapshot replay mass deficit target"
```

---

### Task 2: Add aggregate deficit replay consistency coverage

**Files:**
- Modify: `tests/golden/snapshot_replay_mass_deficit/test_snapshot_replay_mass_deficit.cpp`
- Reference: `tests/unit/coupling/test_coupling_core_state.cpp`

- [ ] **Step 1: Replace the skeleton with aggregate replay helpers and the first real Golden**

At the top of `test_snapshot_replay_mass_deficit.cpp`, replace the skeleton body with:

```cpp
#include <gtest/gtest.h>

#include "coupling/core/state.hpp"

namespace {
constexpr double kHWet = 1.0e-4;

scau::coupling::core::CouplingState make_aggregate_state() {
    return scau::coupling::core::CouplingState{{
        {.volume = 0.0, .mass_deficit_account = {.volume = 1.0}, .phi_t = 1.0, .h = 0.0, .area = 1.0},
        {.volume = 0.0, .mass_deficit_account = {.volume = 2.0}, .phi_t = 1.0, .h = 0.0, .area = 1.0},
    }};
}

void enqueue_aggregate_sequence(scau::coupling::core::CouplingState& state) {
    state.enqueue_event({
        .exchange_cell_index = 0U,
        .direction = scau::coupling::core::ExchangeDirection::engine_to_surface,
        .volume_delta = 3.0,
        .unmet_volume = 2.0,
    });
    state.enqueue_event({
        .exchange_cell_index = 1U,
        .direction = scau::coupling::core::ExchangeDirection::engine_to_surface,
        .volume_delta = 1.0,
        .repayment_volume = 1.0,
    });
}
}

TEST(SnapshotReplayMassDeficitGolden, AggregateDeficitReplayConsistency) {
    auto fresh = make_aggregate_state();
    auto replayed = make_aggregate_state();

    const auto baseline = fresh.compute_system_mass(kHWet);
    const auto replay_snapshot = replayed.snapshot();

    enqueue_aggregate_sequence(fresh);
    fresh.replay_pending();

    replayed.enqueue_event({
        .exchange_cell_index = 0U,
        .direction = scau::coupling::core::ExchangeDirection::engine_to_surface,
        .volume_delta = 999.0,
        .unmet_volume = 999.0,
    });
    replayed.replay_pending();
    replayed.rollback(replay_snapshot);
    enqueue_aggregate_sequence(replayed);
    replayed.replay_pending();

    ASSERT_EQ(fresh.cells().size(), replayed.cells().size());
    for (std::size_t i = 0; i < fresh.cells().size(); ++i) {
        EXPECT_DOUBLE_EQ(fresh.cells()[i].volume, replayed.cells()[i].volume);
        EXPECT_DOUBLE_EQ(
            fresh.cells()[i].mass_deficit_account.volume,
            replayed.cells()[i].mass_deficit_account.volume);
    }

    const auto fresh_delta = fresh.audit_system_mass_against_reference(baseline, kHWet);
    const auto replayed_delta = replayed.audit_system_mass_against_reference(baseline, kHWet);
    EXPECT_DOUBLE_EQ(fresh_delta.current.total_mass, replayed_delta.current.total_mass);
    EXPECT_DOUBLE_EQ(fresh_delta.residual, replayed_delta.residual);
}
```

- [ ] **Step 2: Run the single Golden to verify it passes**

Run:

```bash
cmake --build --preset windows-msvc --target test_snapshot_replay_mass_deficit
ctest --preset windows-msvc -C Debug -R "^test_snapshot_replay_mass_deficit$" -V
```

Expected: PASS with `AggregateDeficitReplayConsistency` green.

- [ ] **Step 3: Commit**

```bash
git add tests/golden/snapshot_replay_mass_deficit/test_snapshot_replay_mass_deficit.cpp
git commit -m "test(golden): cover aggregate deficit replay consistency"
```

---

### Task 3: Add shared-endpoint deficit replay consistency

**Files:**
- Modify: `tests/golden/snapshot_replay_mass_deficit/test_snapshot_replay_mass_deficit.cpp`
- Reference: `tests/unit/coupling/test_coupling_core_state.cpp`

- [ ] **Step 1: Add shared-endpoint state and sequence helpers**

Append these helpers above the tests:

```cpp
scau::coupling::core::CouplingState make_shared_state() {
    return scau::coupling::core::CouplingState{{
        {
            .volume = 0.0,
            .phi_t = 1.0,
            .h = 0.0,
            .area = 1.0,
            .shared_deficit_accounts = {
                {
                    .endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::drainage, .node_id = 10U},
                    .mass_deficit_account = {.volume = 4.0},
                },
                {
                    .endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::river, .node_id = 20U},
                    .mass_deficit_account = {.volume = 1.5},
                },
            },
        },
    }};
}

void enqueue_shared_sequence(scau::coupling::core::CouplingState& state) {
    state.enqueue_event({
        .exchange_cell_index = 0U,
        .direction = scau::coupling::core::ExchangeDirection::engine_to_surface,
        .volume_delta = 2.0,
        .unmet_volume = 2.0,
        .repayment_volume = 1.0,
        .shared_endpoint_events = {
            {
                .endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::drainage, .node_id = 10U},
                .unmet_volume = 1.5,
            },
            {
                .endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::river, .node_id = 20U},
                .unmet_volume = 0.5,
                .repayment_volume = 1.0,
            },
        },
    });
}
```

- [ ] **Step 2: Add the shared replay Golden**

Append this test:

```cpp
TEST(SnapshotReplayMassDeficitGolden, SharedEndpointDeficitReplayConsistency) {
    auto fresh = make_shared_state();
    auto replayed = make_shared_state();

    const auto baseline = fresh.compute_system_mass(kHWet);
    const auto replay_snapshot = replayed.snapshot();

    enqueue_shared_sequence(fresh);
    fresh.replay_pending();

    replayed.enqueue_event({
        .exchange_cell_index = 0U,
        .direction = scau::coupling::core::ExchangeDirection::engine_to_surface,
        .volume_delta = 999.0,
        .unmet_volume = 999.0,
        .shared_endpoint_events = {{
            .endpoint = {.engine = scau::coupling::core::SharedExchangeEngine::drainage, .node_id = 10U},
            .unmet_volume = 999.0,
        }},
    });
    replayed.replay_pending();
    replayed.rollback(replay_snapshot);
    enqueue_shared_sequence(replayed);
    replayed.replay_pending();

    ASSERT_EQ(fresh.cells().size(), replayed.cells().size());
    ASSERT_EQ(fresh.cells()[0].shared_deficit_accounts.size(), replayed.cells()[0].shared_deficit_accounts.size());
    for (std::size_t i = 0; i < fresh.cells()[0].shared_deficit_accounts.size(); ++i) {
        EXPECT_EQ(
            fresh.cells()[0].shared_deficit_accounts[i].endpoint.engine,
            replayed.cells()[0].shared_deficit_accounts[i].endpoint.engine);
        EXPECT_EQ(
            fresh.cells()[0].shared_deficit_accounts[i].endpoint.node_id,
            replayed.cells()[0].shared_deficit_accounts[i].endpoint.node_id);
        EXPECT_DOUBLE_EQ(
            fresh.cells()[0].shared_deficit_accounts[i].mass_deficit_account.volume,
            replayed.cells()[0].shared_deficit_accounts[i].mass_deficit_account.volume);
    }

    ASSERT_EQ(fresh.cells().size(), 1U);
    ASSERT_EQ(fresh.cells()[0].shared_deficit_accounts.size(), 2U);
    EXPECT_DOUBLE_EQ(fresh.cells()[0].volume, 2.0);
    EXPECT_DOUBLE_EQ(fresh.cells()[0].shared_deficit_accounts[0].mass_deficit_account.volume, 5.5);
    EXPECT_DOUBLE_EQ(fresh.cells()[0].shared_deficit_accounts[1].mass_deficit_account.volume, 1.0);
    EXPECT_DOUBLE_EQ(replayed.cells()[0].volume, 2.0);
    EXPECT_DOUBLE_EQ(replayed.cells()[0].shared_deficit_accounts[0].mass_deficit_account.volume, 5.5);
    EXPECT_DOUBLE_EQ(replayed.cells()[0].shared_deficit_accounts[1].mass_deficit_account.volume, 1.0);

    const auto fresh_delta = fresh.audit_system_mass_against_reference(baseline, kHWet);
    const auto replayed_delta = replayed.audit_system_mass_against_reference(baseline, kHWet);
    EXPECT_DOUBLE_EQ(fresh_delta.current.surface_mass, 2.0);
    EXPECT_DOUBLE_EQ(fresh_delta.current.deficit_mass, 6.5);
    EXPECT_DOUBLE_EQ(fresh_delta.current.total_mass, 8.5);
    EXPECT_DOUBLE_EQ(fresh_delta.residual, 3.0);
    EXPECT_FALSE(fresh_delta.conserved);
    EXPECT_DOUBLE_EQ(replayed_delta.current.surface_mass, 2.0);
    EXPECT_DOUBLE_EQ(replayed_delta.current.deficit_mass, 6.5);
    EXPECT_DOUBLE_EQ(replayed_delta.current.total_mass, 8.5);
    EXPECT_DOUBLE_EQ(replayed_delta.residual, 3.0);
    EXPECT_FALSE(replayed_delta.conserved);
    EXPECT_DOUBLE_EQ(fresh_delta.current.total_mass, replayed_delta.current.total_mass);
    EXPECT_DOUBLE_EQ(fresh_delta.residual, replayed_delta.residual);
}
```

- [ ] **Step 3: Run the Golden again**

Run:

```bash
cmake --build --preset windows-msvc --target test_snapshot_replay_mass_deficit
ctest --preset windows-msvc -C Debug -R "^test_snapshot_replay_mass_deficit$" -V
```

Expected: PASS with both aggregate and shared tests green.

- [ ] **Step 4: Commit**

```bash
git add tests/golden/snapshot_replay_mass_deficit/test_snapshot_replay_mass_deficit.cpp
git commit -m "test(golden): cover shared endpoint deficit replay"
```

---

### Task 4: Add pending-event snapshot fidelity and runtime-counter equivalence

**Files:**
- Modify: `tests/golden/snapshot_replay_mass_deficit/test_snapshot_replay_mass_deficit.cpp`
- Reference: `tests/unit/coupling/test_coupling_core_state.cpp`

- [ ] **Step 1: Add a pending-event fidelity Golden**

Append:

```cpp
TEST(SnapshotReplayMassDeficitGolden, PendingEventSnapshotFidelity) {
    auto state = make_aggregate_state();
    state.enqueue_event({
        .exchange_cell_index = 0U,
        .direction = scau::coupling::core::ExchangeDirection::engine_to_surface,
        .volume_delta = 2.0,
        .unmet_volume = 1.0,
    });

    const auto snapshot = state.snapshot();
    state.enqueue_event({
        .exchange_cell_index = 1U,
        .direction = scau::coupling::core::ExchangeDirection::engine_to_surface,
        .volume_delta = 3.0,
        .repayment_volume = 0.5,
    });
    state.rollback(snapshot);

    ASSERT_EQ(snapshot.pending_events().size(), 1U);
    EXPECT_DOUBLE_EQ(snapshot.pending_events()[0].volume_delta, 2.0);
    EXPECT_DOUBLE_EQ(snapshot.pending_events()[0].unmet_volume, 1.0);
    EXPECT_DOUBLE_EQ(snapshot.pending_events()[0].repayment_volume, 0.0);

    state.replay_pending();
    EXPECT_DOUBLE_EQ(state.cells()[0].volume, 2.0);
    EXPECT_DOUBLE_EQ(state.cells()[0].mass_deficit_account.volume, 2.0);
    EXPECT_DOUBLE_EQ(state.cells()[1].volume, 0.0);
    EXPECT_DOUBLE_EQ(state.cells()[1].mass_deficit_account.volume, 2.0);
}
```

- [ ] **Step 2: Add runtime-counter replay equivalence**

Append:

```cpp
TEST(SnapshotReplayMassDeficitGolden, RuntimeCountersMatchAcrossFreshAndReplayPaths) {
    auto fresh = make_aggregate_state();
    auto replayed = make_aggregate_state();
    const auto snapshot = replayed.snapshot();

    auto drive = [](scau::coupling::core::CouplingState& state) {
        enqueue_aggregate_sequence(state);
        state.replay_pending();
        state.record_pipeline_decision({.drain_split_engaged = true});
        state.record_pipeline_decision({.negative_depth_fix_engaged = true});
    };

    drive(fresh);
    replayed.record_pipeline_decision({.drain_split_engaged = true, .negative_depth_fix_engaged = true});
    replayed.rollback(snapshot);
    drive(replayed);

    EXPECT_EQ(fresh.runtime_counters().count_drain_split, replayed.runtime_counters().count_drain_split);
    EXPECT_EQ(
        fresh.runtime_counters().count_negative_depth_fix,
        replayed.runtime_counters().count_negative_depth_fix);
}
```

- [ ] **Step 3: Run the target again**

Run:

```bash
cmake --build --preset windows-msvc --target test_snapshot_replay_mass_deficit
ctest --preset windows-msvc -C Debug -R "^test_snapshot_replay_mass_deficit$" -V
```

Expected: PASS with all Golden cases green.

- [ ] **Step 4: Commit**

```bash
git add tests/golden/snapshot_replay_mass_deficit/test_snapshot_replay_mass_deficit.cpp
git commit -m "test(golden): audit pending event and counter replay fidelity"
```

---

### Task 5: Promote manifest G10 from pending to implemented

**Files:**
- Modify: `tests/golden/suite_manifest/goldensuite.json`
- Verify: `tests/golden/suite_manifest/check_manifest.py`

- [ ] **Step 1: Update the G10 row in the manifest**

In `tests/golden/suite_manifest/goldensuite.json`, change the G10 object from:

```json
{
  "test_id": "G10",
  "name": "snapshot_replay_mass_deficit",
  "test_path": "tests/golden/snapshot_replay_mass_deficit/",
  "applicable_phase": "Phase 1+",
  "reference_path": "tests/golden/reference/tolerances.md",
  "ci_gate": false,
  "status": "pending"
}
```

to:

```json
{
  "test_id": "G10",
  "name": "snapshot_replay_mass_deficit",
  "test_path": "tests/golden/snapshot_replay_mass_deficit/",
  "applicable_phase": "Phase 1+",
  "reference_path": "tests/golden/reference/tolerances.md",
  "ci_gate": false,
  "status": "implemented"
}
```

- [ ] **Step 2: Run the manifest checker**

Run:

```bash
python tests/golden/suite_manifest/check_manifest.py
```

Expected: `OK goldensuite manifest completeness passes`

- [ ] **Step 3: Run the golden subset**

Run:

```bash
ctest --preset windows-msvc -C Debug -L golden --output-on-failure
```

Expected: PASS, now including `test_snapshot_replay_mass_deficit`.

- [ ] **Step 4: Commit**

```bash
git add tests/golden/suite_manifest/goldensuite.json tests/golden/snapshot_replay_mass_deficit/CMakeLists.txt tests/golden/snapshot_replay_mass_deficit/test_snapshot_replay_mass_deficit.cpp tests/golden/CMakeLists.txt
git commit -m "test(golden): implement G10 snapshot replay mass deficit"
```

---

### Task 6: Archive evidence and update index

**Files:**
- Create: `docs/superpowers/specs/2026-06-27-g10-snapshot-replay-mass-deficit-evidence.md`
- Modify: `superpowers/INDEX.md`

- [ ] **Step 1: Write the evidence document**

Create `docs/superpowers/specs/2026-06-27-g10-snapshot-replay-mass-deficit-evidence.md` with this content:

```markdown
# G10 Snapshot Replay Mass Deficit Evidence

## Scope

G10 is now implemented as a pure in-memory CouplingLib core Golden.

It verifies:

- aggregate deficit replay consistency
- shared endpoint deficit replay consistency
- pending event snapshot fidelity
- runtime-counter replay equivalence
- system mass audit equivalence across fresh-vs-replayed execution

## Boundary

This implementation does not claim real 1D solver transient replay correctness. It covers only core-owned replayable state.

## Result

- `python tests/golden/suite_manifest/check_manifest.py` passes
- `ctest -L golden` passes with G10 included
- full suite passes

## Gate recommendation

G10 is promoted to `status: implemented` while keeping `ci_gate: false` for the first landing. Promote to `ci_gate: true` only after one stable CI cycle on the merged branch.
```

- [ ] **Step 2: Add the evidence entry to INDEX**

In `superpowers/INDEX.md`, insert a new stage-evidence row near the other surface/coupling evidence entries:

```markdown
| `docs/superpowers/specs/2026-06-27-g10-snapshot-replay-mass-deficit-evidence.md` | G10 Snapshot Replay Mass Deficit 证据；将 `snapshot_replay_mass_deficit` 从 manifest `pending` 提升为 `implemented`，以纯内存 CouplingLib core Golden 覆盖 aggregate/shared deficit、pending event fidelity 与 system mass audit replay 一致性；`ci_gate` 首轮保持 false。 |
```

- [ ] **Step 3: Run full verification**

Run:

```bash
cmake --build --preset windows-msvc
ctest --preset windows-msvc -C Debug --timeout 120
```

Expected: PASS.

- [ ] **Step 4: Commit**

```bash
git add docs/superpowers/specs/2026-06-27-g10-snapshot-replay-mass-deficit-evidence.md superpowers/INDEX.md
git commit -m "docs(tests): archive G10 snapshot replay mass deficit evidence"
```

---

### Task 7: Final branch verification and handoff

**Files:**
- Verify only

- [ ] **Step 1: Re-run the exact Golden path used by this feature**

Run:

```bash
python tests/golden/suite_manifest/check_manifest.py
ctest --preset windows-msvc -C Debug -R "^test_snapshot_replay_mass_deficit$" -V
ctest --preset windows-msvc -C Debug -L golden --output-on-failure
```

Expected: all PASS.

- [ ] **Step 2: Re-run full suite**

Run:

```bash
ctest --preset windows-msvc -C Debug --timeout 120
```

Expected: PASS.

- [ ] **Step 3: Inspect git status**

Run:

```bash
git status --short
```

Expected: clean working tree.

- [ ] **Step 4: Commit any last doc/test-only fix if needed**

If and only if verification required a last small fix, commit it with a focused message. Otherwise skip this step.

- [ ] **Step 5: Create PR-ready summary notes**

Prepare these bullets for the eventual PR body:

```text
- Implement G10 as a pure in-memory CouplingLib core Golden.
- Cover aggregate/shared deficit replay, pending-event fidelity, runtime counters, and system mass audit equivalence.
- Promote manifest G10 to implemented while keeping ci_gate false for first landing.
```

---

## Self-Review

- Spec coverage: the plan implements all four approved scenario classes plus manifest promotion and evidence archival.
- Placeholder scan: no unresolved placeholders remain; all steps contain exact files, commands, and concrete test content.
- Type consistency: all API names (`snapshot`, `rollback`, `replay_pending`, `compute_system_mass`, `audit_system_mass_against_reference`) match current code.
