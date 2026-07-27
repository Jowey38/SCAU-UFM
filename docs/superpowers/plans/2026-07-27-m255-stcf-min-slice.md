# M255 STCF Minimal Slice Implementation Plan (schema + validation + NetCDF I/O)

Date: 2026-07-27
Base: master `03ba536`
Branch: `feat/m255-stcf-min-slice`

## Goal

Create `libs/stcf/` as the owner of the STCF v5 data contract so the model can
consume real case data instead of programmatically constructed fixtures. This
is the first slice of the Surface2D data-pipeline track (G7 direction) agreed
after the 2026-07-27 progress audit: coupling side is at its phase exit
(G1-G20), the bottleneck is the 2D data path.

## Authority

- Main spec `2026-04-11-scau-ufm-global-architecture-design.md`:
  - §4.2 CellSTCF/EdgeSTCF schema and constraints (SPD `Phi_c`, `phi_t >= max
    diag`, `phi_t <= 1`, lambda bounds `1e-6..1`, cond `<= 1e4`).
  - §9.2 `.stcf.nc` MUST carry `schema_version = 5`; DPM consistency is a
    PreProc-time gate, never a runtime dependency.
  - legacy.10: staged validation (after phi_t, after Phi_c, after omega_edge,
    before export); any failure stops the pipeline fail-closed.
  - §11.2 defaults: `epsilon_phi=1e-6`, `cond_max=1e4`, `epsilon_det=1e-10`.
- Layout spec `project-layout-design.md` §4.10: `libs/stcf/` owns STCF data
  structures, parameter consistency, and NetCDF I/O; it must NOT contain HLLC,
  1D adapters, or scheduling. `libs/surface2d/` consumes stcf (dependency
  direction: surface2d -> stcf, never the reverse).
- Symbols reference: `phi_t`, `Phi_c`, `phi_e_n`, `omega_edge` are separate
  machine-facing names; soil LUT names `K_s`, `psi_f`, `theta_s`, `theta_i`.

## Scope (this slice)

1. `vcpkg.json`: add `netcdf-c` with `"default-features": false` (classic
   CDF format only; no HDF5/DAP tail). Rationale: STCF v5 minimal fields are
   scalar doubles/ints per cell/edge; classic format satisfies §9.2 while
   keeping hosted CI build time bounded. Enhanced netCDF-4 becomes a later
   feature when field volume requires it.
2. `libs/stcf/`:
   - `include/stcf/schema.hpp` + `src/schema.cpp`: `CellStcfFields` (SoA:
     `phi_t`, `phi_xx`, `phi_xy`, `phi_yy`, `manning_n`, `z_b`, `soil_type`),
     `EdgeStcfFields` (SoA: `omega_edge`, `phi_e_n`, `phi_et`),
     `SoilParamsEntry` (`K_s`, `psi_f`, `theta_s`, `theta_i`) with
     `kMaxSoilTypes = 16`, `StcfDataset` aggregate, `kSchemaVersion = 5`.
   - `include/stcf/validate.hpp` + `src/validate.cpp`: staged fail-closed
     validation per §4.2/§9.2/legacy.10 with §11.2 defaults. Throws
     `core::...` error with stage + cell/edge index. This intentionally
     re-states the closure-law constraints inside stcf (stcf cannot depend on
     surface2d); `surface2d/dpm/closure_laws` remains the solver-side check.
   - `include/stcf/io_netcdf.hpp` + `src/io_netcdf.cpp`: `write_stcf(path,
     dataset)` / `read_stcf(path)`. Writing validates first (export gate).
     Reading enforces `schema_version == 5`, presence and dimension agreement
     of every variable, and re-validates after load (fail-closed both ways).
3. `tests/unit/stcf/`: schema defaults; validation accept + per-constraint
   reject matrix; NetCDF round-trip byte-stable values; read rejects missing
   `schema_version`, wrong version, missing variable, dimension mismatch.
4. Root CMake + tests CMake wiring.

## Decisions recorded

- `z_b` file variable: the spec does not define the bed-elevation variable
  name/position in `.stcf.nc`. Decision: cell variable named `z_b`, unit m,
  because the solver state (`eta = h + z_b`) and S_topo consume per-cell bed
  elevation and the symbols reference uses `z_b` as the canonical symbol.
  Recorded here as a spec gap; if the main spec later defines a different
  layout this variable migrates under a G7-style mapping report.
- Drag quartet (`drag_theta`, `drag_a_parallel`, `drag_a_perp`, `drag_cd`),
  Chebyshev LUT, `semantic_label`, `topo_moments`, `clogging_factor`,
  `coupling_friction_multiplier`: NOT in this slice. The current solver
  consumes none of them; adding dead schema now would freeze names without a
  consumer. They enter with the drag/source slices that consume them.
- NetCDF dimension names: `cell`, `edge`, `soil_type_entry` (spec silent);
  global attributes: `schema_version` (int), `title` (free text).
- v4->v5 migration (G7 golden) is NOT this slice: it needs an authored v4
  fixture and a mapping-report format. This slice creates the v5 contract G7
  migrates INTO. G7 stays `pending` in the manifest, untouched.

## Non-goals

- No surface2d -> stcf assembly seam (next slice, M256).
- No PreProc python package or CLI yet.
- No GoldenSuite manifest change.

## Test/evidence plan

- Unit suite `tests/unit/stcf/` green under windows-msvc Debug.
- Full repo ctest green (no behavior change to existing targets).
- Evidence doc `superpowers/specs/2026-07-27-m255-stcf-min-slice-evidence.md`
  after landing, INDEX.md entry appended.
