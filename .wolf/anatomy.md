# anatomy.md

> M272/M273 merged additions, 2026-08-11.

## .wolf/

- `buglog.json` — M272/M273 build, runtime, and audit bug records.
- `cerebrum.md` — project learnings including SWMM external-net and D-Flow boundary gaps.
- `memory.md` — chronological action log.

## docs/superpowers/plans/

- `2026-08-07-m272-swmm-external-net.md` — M272 SWMM external-net implementation plan.
- `2026-08-08-m273-dflowfm-external-boundary-spike.md` — M273 D-Flow external-boundary spike plan.
- `2026-08-09-project-completion-execution-plan.md` — gated end-to-end execution plan.

## extern/swmm5/src/solver/include/

- `swmm5_massbal_bridge.h` — ABI-stable SWMM routing totals snapshot.

## spikes/dflowfm/

- `host/dflowfm_spike_host.cpp` — standalone BMI diagnostic host.
- `cases/single_reach_open_boundary/` — authored open-boundary cases and traces.
- `evidence/m273_external_boundary_contract.md` — FAILED/BLOCKED external-boundary evidence.
- `contract/dflowfm_water_balance_v1.h` — M274 fixed-width native water-balance C ABI.
- `bridge/dflowfm_water_balance_bridge.F90` — M274 external-build Fortran bind(C) bridge source.
- `evidence/m274_native_water_balance_feasibility.md` — M274 bridge feasibility and governed-runtime blocker evidence.

## superpowers/specs/

- `2026-08-07-m272-swmm-external-net-evidence.md` — M272 implementation and verification evidence.
- `2026-08-08-g19-promotion-decision-evidence.md` — G19 BLOCKED decision and G27 prerequisites.

## tests/golden/swmm_external_net/

- `CMakeLists.txt` — G26 non-gating Golden registration.
- `test_swmm_external_net.cpp` — concrete SWMM external-net evidence.

## tests/unit/coupling/

- `test_coupling_swmm_external_net.cpp` — SWMM routing component and API-lateral unit coverage.

## third_party/patches/

- `swmm5-routing-totals.md` — governed SWMM 5.2.4 bridge patch record.
