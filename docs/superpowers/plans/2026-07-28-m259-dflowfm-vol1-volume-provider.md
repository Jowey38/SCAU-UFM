# M259 D-Flow FM `vol1` Volume Provider Plan

Date: 2026-07-28
Base: master `9062efc` (M258 `vol1` contract evidence merged as PR #56)
Branch: `feat/m259-dflowfm-vol1-provider`

## Goal

Turn the M258 real-runtime contract (`V_dflowfm = sum(vol1)`, sampled after
`update()` returns) into a production read path that a case-owned
whole-system mass audit can consume.

## Architecture

1. Extend `IDFlowFMEngine` with a generic state-read method:
   `get_rank1_double_values(var_name)`. This remains lifecycle/state I/O and
   does not add mass, residual, gate, rollback, replay, or arbitration
   semantics to the engine boundary.
2. `DFlowFMEngine` loads BMI 1.0 `get_var_type`, `get_var_rank`, and
   `get_var_shape`; it requires `double`, then rank 1, then queries shape.
   Shape is deliberately queried only after rank==1 because real evidence
   shows some rank>1 shape calls access-violate. It copies the engine-owned
   buffer immediately before another BMI call can invalidate it.
3. Driver-owned `observe_dflowfm_volume()` reads the fixed, evidence-confirmed
   rank-1 variable `vol1`, validates every control volume finite and
   non-negative, and uses Neumaier compensated summation. The name is not
   configurable because no other variable has complete-scope evidence. It returns a DTO
   with `volume`, `sample_count`, `scope_complete`, and `variable_name`; it
   never decides conservation status.
4. `MockDFlowFMEngine` gains generic rank-1 fixtures. The fake BMI runtime
   exposes type/rank/shape for `vol1` and tests the concrete runtime adapter.
5. G11 real golden samples the production provider at initialization and
   after every step, enforces stable node count and finite/non-negative
   storage, and checks the no-lateral authored case has <=1e-9 m3 drift.

## Scope boundary

A successful observation makes the D-Flow FM storage component complete for
the authored/case-owned `vol1` contract only. It does not make the entire
three-engine report complete: SWMM public API still lacks link storage
(`Link.newVolume`), so any combined report must remain `REVIEW_REQUIRED`
until a case-owned complete SWMM provider exists.

## Verification

- Unit: concrete fake-BMI array read, provider sum/compensation, confirmed
  variable missing, uninitialized, non-finite, and negative failures.
- Full repository ctest under windows-msvc Debug.
- Real G11 against release dflowfm.dll and authored single_reach_1d case.
