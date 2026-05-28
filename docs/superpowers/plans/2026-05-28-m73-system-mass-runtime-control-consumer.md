# M73 System Mass Runtime Control Consumer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the minimal coupling-core consumer boundary that turns `SystemMassRuntimeControlDecision.should_abort` into an explicit continue/abort runtime-control result.

**Architecture:** Extend the existing coupling core state API in place, near the current system-mass runtime-control helpers. Add one result enum, one result DTO, and one pure helper that preserves the original decision while exposing the consumed continue/abort state. Test the helper through reference and snapshot system-mass decisions without adding process termination, rollback/replay, or adapter integration.

**Tech Stack:** C++20, GoogleTest, CMake preset `windows-msvc`, existing `scau::coupling::core` namespace.

---

## File Structure

- Modify: `libs/coupling/core/include/coupling/core/state.hpp`
  - Add `SystemMassRuntimeControlState` enum.
  - Add `SystemMassRuntimeControlResult` DTO.
  - Declare `consume_system_mass_runtime_control_decision(const SystemMassRuntimeControlDecision&)`.
- Modify: `libs/coupling/core/src/state.cpp`
  - Implement the pure consumer helper next to the existing runtime-control helper functions.
- Modify: `tests/unit/coupling/test_coupling_core_state.cpp`
  - Add failing tests for reference/snapshot and conserved/drifted paths.
- Create: `superpowers/specs/2026-05-28-m73-system-mass-runtime-control-consumer-evidence.md`
  - Record local focused and full test evidence after implementation.

---

### Task 1: Add failing tests for the consumer boundary

**Files:**
- Modify: `tests/unit/coupling/test_coupling_core_state.cpp`

- [ ] **Step 1: Append the failing tests after the M72 runtime-control audit-chain tests**

Add this block after `RuntimeControlAgainstSnapshotMatchesAuditChainWhenDrifted`:

```cpp
TEST(CouplingCoreState, RuntimeControlConsumerAgainstReferenceContinuesWhenMassIsConserved) {
    constexpr double kHWet = 1.0e-4;
    scau::coupling::core::CouplingState state{{{
        .volume = 10.0,
        .mass_deficit_account = {.volume = 1.0},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    }}};

    const auto baseline = state.compute_system_mass(kHWet);
    const auto decision = state.decide_system_mass_runtime_control_against_reference(baseline, kHWet);

    const auto result = scau::coupling::core::consume_system_mass_runtime_control_decision(decision);

    EXPECT_EQ(result.state, scau::coupling::core::SystemMassRuntimeControlState::continue_run);
    EXPECT_FALSE(result.decision.should_abort);
    EXPECT_EQ(result.decision.gate_outcome.status, scau::coupling::core::SystemMassRuntimeGateStatus::running);
    EXPECT_EQ(result.decision.gate_outcome.decision.action, scau::coupling::core::SystemMassGateAction::continue_run);
    EXPECT_EQ(result.decision.gate_outcome.decision.diagnostic.status, scau::coupling::core::SystemMassConservationStatus::conserved);
    EXPECT_DOUBLE_EQ(result.decision.gate_outcome.decision.diagnostic.residual, 0.0);
    EXPECT_DOUBLE_EQ(result.decision.gate_outcome.decision.diagnostic.baseline_total_mass, 41.0);
    EXPECT_DOUBLE_EQ(result.decision.gate_outcome.decision.diagnostic.current_total_mass, 41.0);
    EXPECT_EQ(result.decision.handling_state, scau::coupling::core::SystemMassRuntimeAbortHandlingState::continue_run);
}

TEST(CouplingCoreState, RuntimeControlConsumerAgainstReferenceAbortsWhenMassDrifted) {
    constexpr double kHWet = 1.0e-4;
    scau::coupling::core::CouplingState state{{{
        .volume = 10.0,
        .mass_deficit_account = {.volume = 1.0},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    }}};

    const auto baseline = state.compute_system_mass(kHWet);
    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 0.0, .unmet_volume = 4.0});
    state.replay_pending();
    const auto decision = state.decide_system_mass_runtime_control_against_reference(baseline, kHWet);

    const auto result = scau::coupling::core::consume_system_mass_runtime_control_decision(decision);

    EXPECT_EQ(result.state, scau::coupling::core::SystemMassRuntimeControlState::abort);
    EXPECT_TRUE(result.decision.should_abort);
    EXPECT_EQ(result.decision.gate_outcome.status, scau::coupling::core::SystemMassRuntimeGateStatus::abort);
    EXPECT_EQ(result.decision.gate_outcome.decision.action, scau::coupling::core::SystemMassGateAction::abort_run);
    EXPECT_EQ(result.decision.gate_outcome.decision.diagnostic.status, scau::coupling::core::SystemMassConservationStatus::drifted);
    EXPECT_DOUBLE_EQ(result.decision.gate_outcome.decision.diagnostic.residual, 4.0);
    EXPECT_DOUBLE_EQ(result.decision.gate_outcome.decision.diagnostic.baseline_total_mass, 41.0);
    EXPECT_DOUBLE_EQ(result.decision.gate_outcome.decision.diagnostic.current_total_mass, 45.0);
    EXPECT_EQ(result.decision.handling_state, scau::coupling::core::SystemMassRuntimeAbortHandlingState::abort);
}

TEST(CouplingCoreState, RuntimeControlConsumerAgainstSnapshotContinuesWhenMassIsConserved) {
    constexpr double kHWet = 1.0e-4;
    scau::coupling::core::CouplingState state{{{
        .volume = 10.0,
        .mass_deficit_account = {.volume = 1.0},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    }}};

    const auto baseline = state.snapshot();
    const auto decision = state.decide_system_mass_runtime_control_against_snapshot(baseline, kHWet);

    const auto result = scau::coupling::core::consume_system_mass_runtime_control_decision(decision);

    EXPECT_EQ(result.state, scau::coupling::core::SystemMassRuntimeControlState::continue_run);
    EXPECT_FALSE(result.decision.should_abort);
    EXPECT_EQ(result.decision.gate_outcome.status, scau::coupling::core::SystemMassRuntimeGateStatus::running);
    EXPECT_EQ(result.decision.gate_outcome.decision.action, scau::coupling::core::SystemMassGateAction::continue_run);
    EXPECT_EQ(result.decision.gate_outcome.decision.diagnostic.status, scau::coupling::core::SystemMassConservationStatus::conserved);
    EXPECT_DOUBLE_EQ(result.decision.gate_outcome.decision.diagnostic.residual, 0.0);
    EXPECT_DOUBLE_EQ(result.decision.gate_outcome.decision.diagnostic.baseline_total_mass, 41.0);
    EXPECT_DOUBLE_EQ(result.decision.gate_outcome.decision.diagnostic.current_total_mass, 41.0);
    EXPECT_EQ(result.decision.handling_state, scau::coupling::core::SystemMassRuntimeAbortHandlingState::continue_run);
}

TEST(CouplingCoreState, RuntimeControlConsumerAgainstSnapshotAbortsWhenMassDrifted) {
    constexpr double kHWet = 1.0e-4;
    scau::coupling::core::CouplingState state{{{
        .volume = 10.0,
        .mass_deficit_account = {.volume = 1.0},
        .phi_t = 0.4,
        .h = 2.0,
        .area = 50.0,
    }}};

    const auto baseline = state.snapshot();
    state.enqueue_event({.exchange_cell_index = 0U, .volume_delta = 0.0, .unmet_volume = 4.0});
    state.replay_pending();
    const auto decision = state.decide_system_mass_runtime_control_against_snapshot(baseline, kHWet);

    const auto result = scau::coupling::core::consume_system_mass_runtime_control_decision(decision);

    EXPECT_EQ(result.state, scau::coupling::core::SystemMassRuntimeControlState::abort);
    EXPECT_TRUE(result.decision.should_abort);
    EXPECT_EQ(result.decision.gate_outcome.status, scau::coupling::core::SystemMassRuntimeGateStatus::abort);
    EXPECT_EQ(result.decision.gate_outcome.decision.action, scau::coupling::core::SystemMassGateAction::abort_run);
    EXPECT_EQ(result.decision.gate_outcome.decision.diagnostic.status, scau::coupling::core::SystemMassConservationStatus::drifted);
    EXPECT_DOUBLE_EQ(result.decision.gate_outcome.decision.diagnostic.residual, 4.0);
    EXPECT_DOUBLE_EQ(result.decision.gate_outcome.decision.diagnostic.baseline_total_mass, 41.0);
    EXPECT_DOUBLE_EQ(result.decision.gate_outcome.decision.diagnostic.current_total_mass, 45.0);
    EXPECT_EQ(result.decision.handling_state, scau::coupling::core::SystemMassRuntimeAbortHandlingState::abort);
}
```

- [ ] **Step 2: Run the focused test to verify it fails to compile**

Run:

```bash
cmake --build --preset windows-msvc --target test_coupling_core_state
```

Expected: FAIL with compiler errors naming missing `consume_system_mass_runtime_control_decision`, `SystemMassRuntimeControlState`, or `SystemMassRuntimeControlResult`.

---

### Task 2: Add the consumer API declaration

**Files:**
- Modify: `libs/coupling/core/include/coupling/core/state.hpp`

- [ ] **Step 1: Add the enum, DTO, and function declaration after `SystemMassRuntimeControlDecision`**

Replace this existing block:

```cpp
struct SystemMassRuntimeControlDecision {
    SystemMassRuntimeGateOutcome gate_outcome{};
    SystemMassRuntimeAbortHandlingState handling_state{SystemMassRuntimeAbortHandlingState::continue_run};
    bool should_abort{false};
};

[[nodiscard]] SystemMassRuntimeAbortHandlingState classify_system_mass_runtime_abort_handling(
```

With:

```cpp
struct SystemMassRuntimeControlDecision {
    SystemMassRuntimeGateOutcome gate_outcome{};
    SystemMassRuntimeAbortHandlingState handling_state{SystemMassRuntimeAbortHandlingState::continue_run};
    bool should_abort{false};
};

enum class SystemMassRuntimeControlState {
    continue_run,
    abort,
};

struct SystemMassRuntimeControlResult {
    SystemMassRuntimeControlDecision decision{};
    SystemMassRuntimeControlState state{SystemMassRuntimeControlState::continue_run};
};

[[nodiscard]] SystemMassRuntimeControlResult consume_system_mass_runtime_control_decision(
    const SystemMassRuntimeControlDecision& decision);

[[nodiscard]] SystemMassRuntimeAbortHandlingState classify_system_mass_runtime_abort_handling(
```

- [ ] **Step 2: Run the focused build to verify only implementation is missing**

Run:

```bash
cmake --build --preset windows-msvc --target test_coupling_core_state
```

Expected: FAIL at link time or compile time because `consume_system_mass_runtime_control_decision` is declared but not defined.

---

### Task 3: Implement the minimal consumer helper

**Files:**
- Modify: `libs/coupling/core/src/state.cpp`

- [ ] **Step 1: Add the implementation after `make_system_mass_runtime_control_decision(const SystemMassConservationDiagnostic&)`**

Insert this block after the overload ending with `make_system_mass_runtime_gate_outcome(diagnostic));`:

```cpp
SystemMassRuntimeControlResult consume_system_mass_runtime_control_decision(
    const SystemMassRuntimeControlDecision& decision) {
    return SystemMassRuntimeControlResult{
        .decision = decision,
        .state = decision.should_abort
            ? SystemMassRuntimeControlState::abort
            : SystemMassRuntimeControlState::continue_run,
    };
}
```

- [ ] **Step 2: Build and run the focused test**

Run:

```bash
cmake --build --preset windows-msvc --target test_coupling_core_state && ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_core_state$"
```

Expected: PASS, with output containing:

```text
100% tests passed, 0 tests failed out of 1
```

- [ ] **Step 3: Commit the API and test implementation**

Run:

```bash
git add -- libs/coupling/core/include/coupling/core/state.hpp libs/coupling/core/src/state.cpp tests/unit/coupling/test_coupling_core_state.cpp
git commit -m "$(cat <<'EOF'
test(coupling): consume system mass runtime control decisions

Add the M73 consumer boundary that maps runtime-control decisions into explicit continue/abort results while preserving the original decision for diagnostics.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

Expected: commit succeeds and includes only the header, source, and test files.

---

### Task 4: Record local M73 evidence

**Files:**
- Create: `superpowers/specs/2026-05-28-m73-system-mass-runtime-control-consumer-evidence.md`

- [ ] **Step 1: Run full serial Debug CTest**

Run:

```bash
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Expected: PASS, with output containing:

```text
100% tests passed, 0 tests failed out of 32
```

- [ ] **Step 2: Create the evidence document**

Create `superpowers/specs/2026-05-28-m73-system-mass-runtime-control-consumer-evidence.md` with this content, replacing the focused and full CTest timing lines with the exact output from the commands just run:

```markdown
# M73 System Mass Runtime Control Consumer Evidence

## Scope

M73 adds a minimal production consumer boundary for `SystemMassRuntimeControlDecision`. The consumer maps `should_abort == false` to `SystemMassRuntimeControlState::continue_run` and `should_abort == true` to `SystemMassRuntimeControlState::abort`, while preserving the original decision for diagnostics and audit continuity.

Helpers exercised:

- `CouplingState::decide_system_mass_runtime_control_against_reference(...)`
- `CouplingState::decide_system_mass_runtime_control_against_snapshot(...)`
- `consume_system_mass_runtime_control_decision(const SystemMassRuntimeControlDecision&)`

Asserted invariants:

- conserved reference decision -> consumer result continues;
- drifted reference decision -> consumer result aborts;
- conserved snapshot decision -> consumer result continues;
- drifted snapshot decision -> consumer result aborts;
- consumer result preserves gate outcome status, gate action, diagnostic status, residual, baseline total mass, current total mass, handling state, and `should_abort`.

## Boundary

M73 is a consumer-boundary helper only.

It does not add:

- process termination;
- rollback or replay orchestration;
- snapshot creation requirements;
- SWMM or D-Flow FM integration;
- runtime evidence file writing;
- tolerance or `epsilon_mass` behavior;
- `REVIEW_REQUIRED`, `BLOCK_MERGE`, or `BLOCK_RELEASE` handling.

## Local Evidence

Focused build and test:

```text
cmake --build --preset windows-msvc --target test_coupling_core_state
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -R "^test_coupling_core_state$"
```

Result:

```text
Write the exact focused test result from Task 3 Step 2 here, including the final passed-test summary line.
```

Full serial Debug CTest:

```text
ctest --test-dir build/windows-msvc -C Debug --output-on-failure -j1
```

Result:

```text
Write the exact full serial Debug CTest result from Task 4 Step 1 here, including the final passed-test summary line.
```

## CI Evidence

Not collected in this local M73 pass because the evidence commit has not yet been pushed.

## Review Evidence

Manual boundary review found the M73 change is limited to:

- one coupling-core result enum and DTO;
- one pure helper that consumes `SystemMassRuntimeControlDecision`;
- GoogleTest cases proving reference and snapshot decisions are consumed into continue/abort results under conserved and drifted system-mass states.

The change does not modify adapters, does not terminate the process, does not write runtime evidence files, and does not introduce rollback, replay, tolerance, or release/merge gate handling.
```

- [ ] **Step 3: Replace evidence-result instructions with exact command output**

In the evidence document, replace the sentence that begins `Write the exact focused test result` with the focused result from Task 3 Step 2. Replace the sentence that begins `Write the exact full serial Debug CTest result` with the full CTest result from Task 4 Step 1.

- [ ] **Step 4: Self-check the evidence document**

Run:

```bash
git diff --check -- superpowers/specs/2026-05-28-m73-system-mass-runtime-control-consumer-evidence.md
grep -n "TBD\|TODO\|Q_max_safe\|Write the exact" superpowers/specs/2026-05-28-m73-system-mass-runtime-control-consumer-evidence.md || true
```

Expected: no whitespace errors and no grep matches.

- [ ] **Step 5: Commit the evidence document**

Run:

```bash
git add -- superpowers/specs/2026-05-28-m73-system-mass-runtime-control-consumer-evidence.md
git commit -m "$(cat <<'EOF'
docs(coupling): record m73 runtime control consumer evidence

Record local focused and full Debug test evidence for the M73 runtime-control consumer boundary.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

Expected: commit succeeds and includes only the M73 evidence document.

---

### Task 5: Push and record CI evidence

**Files:**
- Modify: `superpowers/specs/2026-05-28-m73-system-mass-runtime-control-consumer-evidence.md`

- [ ] **Step 1: Push the two M73 implementation/evidence commits**

Run:

```bash
git push
```

Expected: push succeeds and updates `master` on the remote.

- [ ] **Step 2: Wait for CI**

Run:

```bash
gh run list -R Jowey38/SCAU-UFM --branch master --limit 5
gh run watch -R Jowey38/SCAU-UFM the selected run ID --exit-status
gh run view -R Jowey38/SCAU-UFM the selected run ID --json status,conclusion,jobs,headSha,displayTitle
```

Expected: CI run for the M73 evidence commit completes with `conclusion: success`, and jobs `spike-isolation-check`, `linux-gcc`, and `windows-msvc` all have `conclusion: success`.

- [ ] **Step 3: Update CI Evidence**

Replace this block in `superpowers/specs/2026-05-28-m73-system-mass-runtime-control-consumer-evidence.md`:

```markdown
## CI Evidence

Not collected in this local M73 pass because the evidence commit has not yet been pushed.
```

With:

```markdown
## CI Evidence

M73 commits:

```text
the implementation commit hash test(coupling): consume system mass runtime control decisions
the evidence commit hash docs(coupling): record m73 runtime control consumer evidence
```

GitHub Actions CI run:

```text
the selected run ID push master success
```

Jobs:

```text
spike-isolation-check success
linux-gcc success
windows-msvc success
```
```

Use the exact commit hashes and run ID from the commands above.

- [ ] **Step 4: Self-check and commit the CI evidence update**

Run:

```bash
git diff --check -- superpowers/specs/2026-05-28-m73-system-mass-runtime-control-consumer-evidence.md
grep -n "TBD\|TODO\|Q_max_safe\|Not collected\|the selected run ID\|the implementation commit hash\|the evidence commit hash" superpowers/specs/2026-05-28-m73-system-mass-runtime-control-consumer-evidence.md || true
git add -- superpowers/specs/2026-05-28-m73-system-mass-runtime-control-consumer-evidence.md
git commit -m "$(cat <<'EOF'
docs(coupling): record m73 runtime control consumer ci evidence

Record the CI run for the M73 runtime-control consumer boundary after the implementation and local evidence commits landed on master.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

Expected: diff check has no whitespace errors, grep has no matches, and commit succeeds.

- [ ] **Step 5: Push the CI evidence commit and verify CI again**

Run:

```bash
git push
gh run list -R Jowey38/SCAU-UFM --branch master --limit 5
gh run watch -R Jowey38/SCAU-UFM the selected run ID --exit-status
gh run view -R Jowey38/SCAU-UFM the selected run ID --json status,conclusion,jobs,headSha,displayTitle
git fetch origin master && git status --short --branch
```

Expected: final CI run has `conclusion: success`, all three jobs succeed, and local `master...origin/master` is synchronized except for unrelated untracked session export files.
