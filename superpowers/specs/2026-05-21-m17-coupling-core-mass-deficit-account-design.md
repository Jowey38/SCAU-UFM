# M17 CouplingLib Core Mass Deficit Account Design

**Date:** 2026-05-21

## Goal

Add the minimal pure `CouplingLib` core semantics for the `mass_deficit_account` ledger:

- record unmet exchange volume as a positive deficit in `m^3`
- support rolling deficit forward
- support clearing a deficit by applied volume
- support replay-safe snapshotting of the deficit ledger state

M17 keeps this logic inside `libs/coupling/core` and does not connect it to `Surface2DCore`, SWMM, D-Flow FM, shared-cell arbitration, scheduler logic, or fault orchestration.

## Scope

M17 extends the coupling core with only the minimal data needed to model a deficit ledger for one exchange cell or one shared coupling ledger entry.

The pure core should expose:

- a small `MassDeficitAccount` value type
- a minimal update API for rolling deficit forward and applying repayment
- snapshot/rollback/replay compatibility through the same `CouplingState` persistence path used in M15

## API

The public core API gains a small deficit ledger type that can be embedded in the coupling state or carried independently:

```cpp
struct MassDeficitAccount {
    double volume{0.0};
};

[[nodiscard]] MassDeficitAccount roll_deficit(const MassDeficitAccount& account, double unmet_volume);
[[nodiscard]] MassDeficitAccount apply_repayment(const MassDeficitAccount& account, double applied_volume);
```

## Behavior

For valid inputs:

- `roll_deficit(...)` adds unmet positive volume to the existing deficit.
- `apply_repayment(...)` subtracts applied positive volume from the existing deficit.
- deficit volume is clamped at zero on repayment.

Validation boundaries:

- unmet or applied volume must be non-negative
- deficit volume must not become negative

## Tests

Add focused `test_coupling_mass_deficit_account` coverage for:

- rolling a zero deficit forward with unmet volume
- accumulating multiple unmet increments
- repaying part of a deficit
- clamping repayment at zero
- rejecting negative unmet or applied volume

## Boundaries

- Do not add arbitration or priority weight semantics.
- Do not add shared-cell allocation rules.
- Do not add scheduler or fault controller behavior.
- Do not add SWMM or D-Flow FM adapters.
- Do not redefine `Q_limit` or `V_limit`.
- Do not add `epsilon_deficit` tolerances in the correctness path.
