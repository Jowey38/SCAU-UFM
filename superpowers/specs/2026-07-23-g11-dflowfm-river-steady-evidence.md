# G11 dflowfm_river_steady real-runtime evidence

Date: 2026-07-23

Status: `implemented`, `ci_gate:false`.

## Runtime and case

- release `dflowfm.dll` from the local `install_fm-suite_release/bin`;
- project-authored case `spikes/dflowfm/cases/single_reach_1d/`;
- deterministic project-authored UGRID network (SHA-256
  `71efdc09db8d6fa9c04798350a4df0430b2e76de9a2722b6f1eb0af74264a687`);
- native lateral `lat1` at branch chainage 500 m.

## Golden contract

`test_dflowfm_river_steady` uses the real `DFlowFMEngine` and tri-coupling
driver. Project river endpoint 7 maps to native lateral ID `lat1` without
leaking the native string into CouplingLib core.

For 100 substeps at 60 s:

- zero surface request grants zero exchange;
- surface system mass is unchanged each substep;
- `s1[0]` and `q1[0]` remain finite;
- native compound lateral remains zero;
- D-Flow elapsed time is exactly 6000 s.

The local real-runtime test passed in 1.44 s after adding the installed runtime
`bin` directory to `PATH` so dependent DLLs resolve.

## CI boundary

The test explicitly skips when `SCAU_DFLOWFM_LIBRARY` or
`SCAU_DFLOWFM_G11_MDU` is absent. It is therefore implemented but non-gating.
Promotion to `ci_gate:true` requires a governed CI runtime artifact or runner;
no test may silently substitute a mock for this Golden.

## Remaining Phase-2 boundary

BMI 1.0 still has no save/restore state API. Native restart checkpoint reload
must be implemented and evidenced separately before claiming real-engine
rollback/replay parity.
