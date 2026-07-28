# M259 D-Flow FM `vol1` Volume Provider Evidence

Date: 2026-07-28
Base evidence: M258 `spikes/dflowfm/evidence/vol1_volume_contract.md`

## Result

The confirmed real-runtime contract `V_dflowfm = sum(vol1)` now has a
production read path:

- `IDFlowFMEngine::get_rank1_double_values()` is a generic state read; no
  mass/gate/recovery semantics entered the river adapter boundary.
- `DFlowFMEngine` validates BMI type=`double`, rank=1, positive shape, then
  immediately copies the engine-owned buffer. `get_var_shape` is never called
  for rank>1 variables (known release-DLL access-violation boundary).
- `driver::observe_dflowfm_volume()` defaults to `vol1`, rejects empty,
  non-finite, or negative control volumes, and returns a Neumaier compensated
  total with sample count and `scope_complete=true` for D-Flow FM.
- `MockDFlowFMEngine` has generic rank-1 fixtures; the fake BMI exports
  `vol1={10,20,30}` plus BMI type/rank/shape metadata so the concrete runtime
  adapter is tested, not only the mock interface.

## Verification

- windows-msvc Debug full repository suite: **127/127 passed**.
- Targeted runtime/provider suites: 2/2 passed.
- Real release DLL G11, authored `single_reach_1d`, 100 x update(60): PASS.
  The production provider was sampled at initialization and after every
  update; node count stayed stable; every total was finite/non-negative; final
  total matched initial within 1e-9 m3.
- Real-run invocation followed the governed gateway contract: cwd set to the
  case directory and MDU passed as relative `single_reach.mdu`. A long
  absolute MDU path from another cwd returned engine initialize code 21; this
  is an existing D-Flow runtime/case-path constraint, not a provider failure.
- Final build `LNK1168 == 0`.

## Scope / remaining block

D-Flow FM storage scope can now be complete when the case confirms `vol1`.
The complete three-engine audit must still remain `REVIEW_REQUIRED` because
the SWMM public API exposes node storage but not pipe/link `Link.newVolume`.
This change intentionally does not claim whole-system conservation or release
readiness beyond that boundary.
