# M240 Drainage Adapter Baseline Evidence

## Scope

This evidence records the minimum drainage-side baseline added on top of `origin/master` `75a6c25` so that a real roof -> SWMM adapter becomes implementable without breaking the `Surface2DCore` boundary.

The implementation adds:

- `ISwmmEngine`
- `MockSwmmEngine`
- `SwmmRoofDrainageAcceptanceAdapter`

It does not add any real SWMM C API binding or third-party engine build.

## Files

- `libs/coupling/drainage/CMakeLists.txt`
- `libs/coupling/drainage/include/coupling/drainage/swmm_engine.hpp`
- `libs/coupling/drainage/include/coupling/drainage/mock_swmm_engine.hpp`
- `libs/coupling/drainage/include/coupling/drainage/roof_drainage_adapter.hpp`
- `libs/coupling/drainage/src/swmm_engine.cpp`
- `libs/coupling/drainage/src/mock_swmm_engine.cpp`
- `libs/coupling/drainage/src/roof_drainage_adapter.cpp`
- `tests/unit/coupling/test_coupling_mock_swmm_engine.cpp`
- `tests/unit/coupling/test_coupling_roof_drainage_adapter.cpp`
- `tests/unit/coupling/CMakeLists.txt`
- `CMakeLists.txt`

## Boundary result

`surface2d` remains independent from drainage and SWMM ABI concerns.

Verified by search:

- no `coupling/drainage` includes under `libs/surface2d/`
- the new adapter depends on `surface2d` DTOs from the drainage side only
- the roof seam remains `surface2d::RoofDrainageAcceptanceFn`

## Behavior summary

### MockSwmmEngine

- stores node heads, node lateral inflows, surcharge flags, and link flows
- rejects invalid node/link ids by throwing `SwmmEngineError`
- uses override semantics for `set_node_lateral_inflow`
- validates finite values and positive step dt

### SwmmRoofDrainageAcceptanceAdapter

- plugs into the roof acceptance seam from the drainage side
- rejects invalid target nodes fail-closed
- rejects surcharged nodes with `NodeSurcharged`
- converts accepted volume to flow using `q = accepted_volume / dt_sub`
- accumulates per-node flow internally so multiple roof cells mapped to the same SWMM node do not overwrite one another
- clears per-step accumulation with `begin_step()`

## Verification

Executed in clean worktree `H:/githubcode/SCAU-UFM/.worktrees/m240-drainage-baseline`.

### Targeted drainage tests

```text
ctest --preset windows-msvc -R "test_coupling_(mock_swmm_engine|roof_drainage_adapter)" --output-on-failure
```

Result:

```text
2/2 passed, 0 failed
```

### Focused roof/runoff integration checks

```text
ctest --preset windows-msvc -R "test_(coupling_mock_swmm_engine|coupling_roof_drainage_adapter|runoff_roof|runoff_roof_step)$" --output-on-failure
```

Result:

```text
4/4 passed, 0 failed
```

### Full suite

```text
ctest --preset windows-msvc --output-on-failure
```

Result:

```text
57/57 passed, 0 failed
```

## Assessment

The drainage-side baseline now exists and is tested. A real SWMM-backed implementation can be added behind `ISwmmEngine` later without changing the `surface2d` roof seam or violating the `Surface2DCore` boundary.
