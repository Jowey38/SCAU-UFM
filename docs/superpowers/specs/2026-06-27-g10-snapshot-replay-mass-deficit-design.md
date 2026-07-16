# G10 Snapshot Replay Mass Deficit Design

## Status

Design approved in chat.

## Goal

Promote GoldenSuite entry `G10 snapshot_replay_mass_deficit` from `pending` to an implemented Phase-1 Golden built on the existing CouplingLib core replay boundary.

The first version is intentionally a **pure in-memory core Golden**. It does not attempt to prove real 1D solver transient replay across gravity-flow / pressure-flow regime transitions. Instead it proves that CouplingLib's core-owned replayable state is snapshot-safe, rollback-safe, replay-deterministic, and mass-deficit-account consistent.

## Scope Boundary

### In scope

- `snapshot()`
- `rollback(snapshot)`
- `replay_pending()`
- aggregate `mass_deficit_account`
- `shared_deficit_accounts`
- `pending_events`
- `runtime_counters()`
- `compute_system_mass(...)`
- `audit_system_mass_against_reference(...)`
- `SystemMassConservationDiagnostic`

### Out of scope

- real SWMM transient replay semantics
- real D-Flow FM transient replay semantics
- gravity-flow ↔ pressure-flow regime switching inside a 1D solver
- file-backed snapshot persistence
- any external runtime evidence sink, DTO persistence, or publisher path

## Why this is sufficient for G10

The manifest name is `snapshot_replay_mass_deficit`. The minimal credible contract for that name is:

1. snapshot captures the replayable core state;
2. rollback restores that state;
3. replay re-applies pending exchange state deterministically;
4. mass deficit ledgers do not drift across replay paths;
5. system mass audit remains invariant across fresh-vs-replayed execution.

This design satisfies that contract without over-claiming real 1D solver replay guarantees.

The key design consequence is that G10 v1 is a **CouplingLib core Golden**, not a solver-transient or pipe-regime replay study. Any scenario that depends on gravity-flow ↔ pressure-flow switching inside a real 1D engine is explicitly treated as a later, deeper replay investigation rather than part of the first implemented Golden.

## Core Guardrails

### 1. Core-owned replayable state determinism

G10 version 1 requires deterministic equality only for CouplingLib core-owned replayable state:

- cell storage state (`volume`, `h`, `phi_t`, `area` as already represented in `ExchangeCellState`)
- aggregate deficit ledger
- shared endpoint deficit ledgers
- pending events and their replay metadata
- runtime counters
- system mass audit outputs

It does **not** require solver-internal caches, friction intermediates, or external engine transient history to be bitwise identical.

### 2. Deficit ledger closed-loop accounting

All deficit generation, rollover, repayment, rollback, and replay must remain explicit in core state.

Audit must be expressible using existing core APIs, not new side metrics:

- `compute_system_mass(...)`
- `audit_system_mass_against_reference(...)`
- `SystemMassConservationDiagnostic`

Both aggregate and shared-endpoint deficit paths must be covered.

### 3. In-memory snapshot boundary only

Snapshot/replay correctness must rely only on in-memory state objects and deep-copy snapshot semantics.

No temporary JSON, binary dump, or file-backed persistence is allowed in the Golden path.

## Proposed Test Location

Create a dedicated Golden directory:

```text
tests/golden/snapshot_replay_mass_deficit/
```

Contents:

- `CMakeLists.txt`
- `test_snapshot_replay_mass_deficit.cpp`

This test should be wired into the GoldenSuite tree like G1-G6, but for the first landing it must use a non-gating label such as `candidate_non_gating` rather than `golden`, so the existing CI job `ctest -L golden` does not pick it up before explicit gate promotion.

## Scenario Matrix

### Scenario 1 — aggregate deficit replay consistency

Construct a minimal state with:

- aggregate `mass_deficit_account`
- one or more pending replay events
- deterministic baseline mass audit

Run:

- path A: fresh `replay_pending()`
- path B: `snapshot()` -> replay setup mutation -> `rollback(snapshot)` -> `replay_pending()`

Assert equality of:

- final aggregate deficit volume
- final pending-events emptiness / replay completion result
- `compute_system_mass(...)`
- `audit_system_mass_against_reference(...)`

### Scenario 2 — shared endpoint deficit replay consistency

Construct a state with:

- `shared_deficit_accounts` for at least two distinct endpoints
- endpoint-specific pending replay volumes

Assert after fresh-vs-replayed execution:

- each endpoint deficit volume matches individually
- endpoint ordering/identity remains stable
- total system mass matches

### Scenario 3 — pending event snapshot fidelity

Construct a state where snapshot is taken with live pending events already queued.

Assert rollback restores:

- event count
- per-event `volume_delta`
- per-event `unmet_volume`
- per-event `repayment_volume`

Then replay both paths and assert identical outcomes.

### Scenario 4 — system mass audit equivalence

For both aggregate and shared-deficit variants, compare:

- `compute_system_mass(...)`
- `audit_system_mass_against_reference(...)`
- `SystemMassConservationDiagnostic`

between fresh path and rollback+replay path.

The purpose is to elevate G10 from a ledger-only regression into a true audit-level Golden.

## Assertion Strategy

### Prefer exact equality

Use exact checks first:

- `EXPECT_DOUBLE_EQ` for deficit volumes, pending-event fields, and runtime counters
- exact equality for event counts, endpoint identities, and booleans

### When residual APIs are already audit-based

For `SystemMassConservationDiagnostic` and audit residuals, use the existing API semantics.

If the current implementation already produces exact zero residuals, keep `EXPECT_DOUBLE_EQ(0.0)`.
Only drop to `EXPECT_NEAR` if a specific existing core path proves non-bitwise but still deterministic under audit tolerances.

The default expectation for G10 v1 is **deterministic equality**, not fuzzy conservation.

## Manifest / Gate Recommendation

Recommended first landing in `tests/golden/suite_manifest/goldensuite.json`:

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

### Why not `ci_gate: true` immediately

First landing should prove:

- target exists,
- test is reproducible,
- local and CI runners are stable,
- no hidden fixture/platform flake exists.

After one stable CI cycle, promote `ci_gate` to `true` in a follow-up change.

## Files Expected to Change in Implementation

- `tests/golden/CMakeLists.txt`
- `tests/golden/snapshot_replay_mass_deficit/CMakeLists.txt`
- `tests/golden/snapshot_replay_mass_deficit/test_snapshot_replay_mass_deficit.cpp`
- `tests/golden/suite_manifest/goldensuite.json`
- `tests/golden/suite_manifest/check_manifest.py` (only if the current checker needs no-op support for G10 path shape)
- `superpowers/INDEX.md`
- evidence doc for implementation results

Implementation should reuse current core helpers and tests rather than invent a new replay framework.

In particular, the Golden should be assembled by lifting and consolidating the already-proven semantics in:

- `tests/unit/coupling/test_coupling_core_state.cpp`
- `tests/unit/coupling/test_coupling_mass_deficit_account.cpp`
- `compute_system_mass(...)`
- `audit_system_mass_against_reference(...)`

Do **not** introduce a new `tests/unit/pipe1d/` or `libs/pipe1d/` test surface for G10 v1. That would create a new scope and muddle the ownership boundary of the first implemented Golden.

## Explicit Rejections

The following ideas were reviewed and deliberately rejected for G10 v1:

1. **Real pipe1d / single-pipe gravity-flow ↔ pressure-flow replay test**
   - too deep for first landing
   - crosses from CouplingLib core Golden into solver-transient integration territory

2. **File-backed snapshot dumps**
   - violates pure in-memory boundary
   - introduces platform/file-system variance with no benefit for v1

3. **Overloading G10 with failure-revealing candidate semantics**
   - G10 should be a passing implemented Golden in v1
   - failure-revealing designs belong to future deeper replay/solver investigations

## Follow-up After G10 v1

Potential later layers:

- tri-coupling multi-sub-step replay Golden
- real SWMM/D-Flow FM replay boundary tests
- 1D transient failure-revealing replay cases
- CI promotion from `ci_gate: false` to `ci_gate: true`

The previously discussed "failure-revealing" ideas remain valid only as **post-G10-v1 extensions**. If pursued later, they should be framed as a separate candidate or enhancement track (for example, a G10b/M26x replay-transient investigation), not folded back into the first implemented `G10 snapshot_replay_mass_deficit` Golden.

## Validation Plan

Implementation must, at minimum, verify:

```text
python tests/golden/suite_manifest/check_manifest.py
ctest --preset windows-msvc -L golden --output-on-failure
ctest --preset windows-msvc --timeout 120
```

If Windows path-length becomes an issue in a deep worktree, use a short build root as already proven in prior work.
