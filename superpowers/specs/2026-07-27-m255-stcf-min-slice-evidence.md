# M255 STCF Minimal Slice Evidence

Date: 2026-07-27
Branch: `feat/m255-stcf-min-slice` (base master `03ba536`)

## Summary

Created `libs/stcf/` as the STCF v5 data-contract owner: schema structs,
staged fail-closed validation, and NetCDF classic-format I/O. This is the
first slice of the Surface2D data-pipeline track opened after the 2026-07-27
progress audit (coupling side at phase exit G1-G20; the bottleneck is the 2D
data path / G7 direction). No solver behavior changed.

## What landed

- `libs/stcf/include/stcf/schema.hpp` + `src/schema.cpp`: `CellStcfFields`
  (`phi_t`, `phi_xx/xy/yy`, `manning_n`, `z_b`, `soil_type`), `EdgeStcfFields`
  (`omega_edge`, `phi_e_n`, `phi_et`), `SoilParamsEntry` (`K_s`, `psi_f`,
  `theta_s`, `theta_i`), `StcfDataset`, `kSchemaVersion = 5`,
  `kMaxSoilTypes = 16`, `make_uniform_dataset`.
- `libs/stcf/.../validate.*`: staged validation with main-spec section 11.2
  defaults (`epsilon_phi=1e-6`, `cond_max=1e4`, `epsilon_det=1e-10`).
  Enforces `phi_t in (0,1]`, `Phi_c` SPD + eigenvalue bounds `[1e-6,1]` +
  condition number, `phi_t >= max diag`, edge fields in `[0,1]`, soil
  constraints (`psi_f>0`, `theta_i<theta_s<=1`, LUT size <= 16, soil_type in
  range). Error messages name the stage and index.
- `libs/stcf/.../io_netcdf.*`: `write_stcf` validates before creating any
  file (export gate; invalid data never reaches disk); `read_stcf` enforces
  `schema_version == 5`, variable presence, type, and dimension agreement,
  then re-validates the loaded dataset.
- `vcpkg.json`: `netcdf-c` with `default-features:false` (classic CDF).
- `tests/unit/stcf/`: 3 suites, 22 test cases.

## Verification

- Full repo ctest under windows-msvc Debug (short build dir `H:/scau-b-stcf`):
  **123/123 passed, 0 failed** (was 120 baseline + 3 new stcf suites).
- STCF suites: `test_stcf_schema` (4), `test_stcf_validate` (15),
  `test_stcf_io_netcdf` (8) all green. Round-trip is bitwise-exact
  (`EXPECT_EQ` on doubles). Tamper tests confirm read-side fail-closed on
  wrong/absent `schema_version`, missing variable, wrong dimension, and an
  out-of-range value written under the validation gate.
- `LNK1168 == 0` (no stale-exe false results).

## Spec-gap decisions recorded

- `z_b` on-disk variable name/position is undefined by spec; chosen as a
  per-cell `z_b` double (m). Migrates under a G7-style mapping report if the
  main spec later defines a different layout.
- Drag quartet, Chebyshev LUT, semantic labels, topo moments, and the
  transitional `clogging_factor`/`coupling_friction_multiplier` are out of
  scope: no current solver consumer, so no dead schema frozen now.
- NetCDF dimension names `cell`/`edge`/`soil_type_entry` and attributes
  `schema_version`/`title` chosen where the spec is silent.

## Not in this slice

- surface2d <-> stcf assembly seam (M256).
- PreProc python package / CLI.
- G7 `stcf_v4_to_v5_migration` golden (needs an authored v4 fixture + mapping
  report format); it stays `pending` in the manifest, untouched.
