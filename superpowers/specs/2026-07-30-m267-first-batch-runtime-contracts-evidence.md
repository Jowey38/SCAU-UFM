# M267 First-Batch Runtime Contracts Evidence

Date: 2026-07-30
Base: `origin/master@7f24a77`

## SimDriver skeleton

`apps/sim_driver` now owns a versioned C++ `RuntimeConfig` and a fail-closed
lifecycle state machine. It records committed coupling boundaries but does not
copy HLLC, coupling arbitration, SWMM or BMI semantics. The first executable is
an explicit orchestration skeleton; no unreviewed JSON/YAML user schema was
invented.

## Checkpoint coordinator

`coupling/driver/checkpoint_coordinator` validates module-neutral prepared
checkpoint metadata and commits only a unique, same-epoch, same-logical-time
set containing Surface2D, CouplingState and SimDriver plus every enabled 1D
engine. Missing modules, duplicate modules, epoch/time drift or missing
schema/hash/payload metadata abort the record. Module serialization and restore
remain adapter-owned.

## SWMM whole-domain storage

Concrete `SwmmEngine::total_stored_volume()` bridges the locked SWMM 5.2.4
internal `massbal_getStorage(FALSE)` calculation used by upstream mass balance.
It includes node and link `newVolume`, converts internal ft3 to m3 for the
project-required CMS unit system, and rejects invalid results. The generic
`ISwmmEngine` ABI is unchanged; no SWMM struct or enum leaks into public DTOs.

This is a governed internal ABI bridge and must be revalidated on any SWMM
version change.

## External import and mesh quality contracts

`stcf/import_contract` requires authorization, source/target CRS, horizontal and
vertical units, and unique mappings into canonical STCF fields before parsing.
No real importer is claimed because no authorized upstream sample is present.

`mesh/quality` emits review/fatal issues for geometric thresholds without
silent repair. Existing `build_mesh` remains the fatal topology boundary.

## Surface backend contract

`surface2d/backend` exposes CPU reference, deterministic CUDA and performance
CUDA capabilities. CPU dispatch is bitwise identical to the existing reference
step. Both CUDA modes fail before state mutation because `nvcc`, kernels, device
snapshot and G9 evidence are absent. G9 remains pending.

## CVC failure exposure

CVC tests now cover near-dry low-porosity side fluxes, oblique Cartesian
momentum and both flux directions. Physical storage/momentum closure remains
finite for the pure side-fluctuation seam. The production flag is explicitly
verified default-off. This does not constitute arbitrary wet/dry positivity or
high-order CVC proof.

## Narrow verification

The following tests were built and passed before full-suite verification:

- `test_sim_driver`;
- `test_coupling_checkpoint_coordinator`;
- `test_coupling_swmm_engine`;
- `test_stcf_import_contract`;
- `test_mesh_quality`;
- `test_backend_contract`.

## Integrated verification

Windows MSVC Debug at short build root `H:/scau-b-m264`, branch
`feat/m264-first-batch` on `origin/master@7f24a77`:

- full repository suite: **142/142 passed** (baseline 135 + 7 new tests);
- `ctest -L golden`: **21/21 passed**, including the newly promoted
  G7 `stcf_v4_to_v5_migration`, G10 `snapshot_replay_mass_deficit` and
  G12 `dual_engine_shared_cell` gates;
- GoldenSuite manifest completeness: passed;
- final rebuild error count (`LNK1168|LNK2|error C|fatal error`): **0**.

Transient LNK1168 file locks recurred during the first full parallel relink
(known bug-147/150 environment pattern: AV/indexer holds recently executed test
exes). Resolution followed the recorded procedure: kill stray test processes,
delete locked exes by basename, rebuild until the error count is zero. No
source-level defect was involved.

`SCAU_DFLOWFM_LIBRARY` is not configured in this shell, so the four real
D-Flow FM goldens ran their deterministic hosted paths (mock/skip semantics
unchanged); no fresh real-engine claim is added by this evidence.
