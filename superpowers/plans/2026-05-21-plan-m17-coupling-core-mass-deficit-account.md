# M17 CouplingLib Core Mass Deficit Account Plan

> **For agentic workers:** Use the smallest pure `CouplingLib` core slice possible. Keep M17 limited to `mass_deficit_account` ledger semantics only. Do not connect SWMM, D-Flow FM, `Surface2DCore`, arbitration, scheduler, or fault handling.

**Goal:** Add the minimal pure core `mass_deficit_account` ledger semantics for rolling deficit forward and applying repayments.

**Architecture:** Introduce a small deficit ledger value type and pure update functions in `libs/coupling/core`, with focused unit tests under `tests/unit/coupling`. Keep the scope aligned with the existing `CouplingState` snapshot/rollback/replay core, but do not expand to arbitration or engine adapters.

**Tech Stack:** C++20, CMake, GoogleTest, existing `scau::coupling_core` and `scau::warnings` targets.

---

## File Structure

- Modify `libs/coupling/core/include/coupling/core/state.hpp`
  - Adds `MassDeficitAccount` and minimal ledger function declarations.
- Modify `libs/coupling/core/src/state.cpp`
  - Implements deficit roll-forward and repayment logic.
- Create `tests/unit/coupling/test_coupling_mass_deficit_account.cpp`
  - Adds focused M17 regression tests.
- Modify `tests/unit/coupling/CMakeLists.txt`
  - Registers `test_coupling_mass_deficit_account`.

---

## Task 1: Add Mass Deficit Ledger Tests And API

**Files:**
- Modify: `libs/coupling/core/include/coupling/core/state.hpp`
- Modify: `libs/coupling/core/src/state.cpp`
- Create: `tests/unit/coupling/test_coupling_mass_deficit_account.cpp`
- Modify: `tests/unit/coupling/CMakeLists.txt`

- [ ] **Step 1: Create focused mass-deficit tests**

Create `tests/unit/coupling/test_coupling_mass_deficit_account.cpp` with coverage for rolling deficit forward, repaying deficit, clamping at zero, and rejecting negative inputs.

- [ ] **Step 2: Register the focused test target**

Append a `test_coupling_mass_deficit_account` executable and `add_test(...)` entry to `tests/unit/coupling/CMakeLists.txt`.

- [ ] **Step 3: Run the focused test and verify it fails before implementation**

Run the new test target build/executable and confirm compilation or execution fails because the API is not implemented yet.

- [ ] **Step 4: Extend the public API**

In `libs/coupling/core/include/coupling/core/state.hpp`, add a small `MassDeficitAccount` value type and declare the minimal pure ledger update functions.

- [ ] **Step 5: Implement the ledger updates and validation**

In `libs/coupling/core/src/state.cpp`, implement the pure deficit roll-forward and repayment semantics with non-negative validation.

- [ ] **Step 6: Run focused coupling tests**

Run the new `test_coupling_mass_deficit_account` target and the existing coupling core state test target to verify no regression.

---

## Task 2: Validate M17 And Record Evidence

**Files:**
- Create: `superpowers/specs/2026-05-21-plan-m17-coupling-core-mass-deficit-account-evidence.md`

- [ ] **Step 1: Run full CTest**

Run the full test suite and verify all coupling and existing surface tests still pass.

- [ ] **Step 2: Create evidence document**

Create an evidence note recording the observed pass counts and the exact M17 scope.

---

## Boundaries

- Preserve `Q_limit` / `V_limit` as already defined.
- Do not introduce arbitration, `priority_weight`, shared-cell splitting, or engine adapters.
- Do not add scheduler, fault, or write-off logic.
- Do not connect `mass_deficit_account` to SWMM or D-Flow FM adapters in this slice.
