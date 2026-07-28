# M256/M257/G21 Surface2D Data-Pipeline Evidence

Date: 2026-07-28
Branch: `feat/m255-stcf-min-slice` (base master `03ba536`)
Builds on: M255 (`libs/stcf` schema + validation + NetCDF I/O).

## M256 stcf_bridge (STCF -> Surface2D assembly seam)

- `libs/surface2d/stcf_bridge/assemble.*`: `assemble_dpm_fields` (phi_t +
  Phi_c per cell; file-carried `phi_e_n`/`omega_edge` per edge -- main-spec
  5.3 rule-1 PreProc-primary assembly, deliberately NOT re-derived from cell
  tensors), `assemble_source_term_fields` (manning_n; exchange_volume zero --
  coupling sovereignty), `assemble_bed_elevations` (z_b for eta = z_b + h
  initialization). Fail-closed mesh count checks.
- surface2d now links `scau::stcf` PUBLIC (layout-spec dependency direction:
  surface2d consumes stcf; stcf never sees surface2d).
- Tests: `test_stcf_bridge_assemble` (5 cases).

## M257 boundary conditions: DischargeInflow + WaterLevel

- `DischargeInflow`: prescribed q >= 0 [m^2/s] per unit edge length enters
  the inside cell as a source-type mass flux on a wall-pressured edge; q = 0
  reduces bitwise to the Wall branch (lake at rest preserved). New
  `StepDiagnostics.boundary_inflow_volume` audit (zero on rollback). CFL
  treats these edges as walls (no Riemann wave).
- `WaterLevel`: prescribed stage eta_bc builds a ghost state over the inside
  bed (depth max(0, eta_bc - z_b), inside velocity) and feeds the existing
  HLLC boundary path; the ghost pair also assembles the internal-edge WB
  pressure/topo pairing with ghost phi_t = inside phi_t, so a matching stage
  keeps a lake at rest (1e-12) and higher/lower stages drive in-/outflow.
- Defect found and fixed while testing: boundary ghost states were always
  passed as the Riemann LEFT state even when the inside cell is the edge's
  right cell (stored normal points left -> right). Symmetric Open copies
  masked this; the asymmetric WaterLevel ghost exposed it (inflow/outflow
  inverted). Ghost states are now placed on the side the stored normal
  references. Open behavior is unchanged (symmetric states are
  order-independent), confirmed by the full suite.
- Hydrograph/stage time series stay a driver concern: update the per-edge
  prescribed values between steps.
- Tests: `test_boundary_inflow_water_level` (8 cases).

## G21 stcf_case_pipeline (GoldenSuite, ci_gate: true)

Locks the full data path: authored dataset -> `write_stcf` -> `read_stcf` ->
stcf_bridge assembly -> `advance_one_step_cpu`, on the mixed minimal mesh
with a sloping bed, spatially varying phi_t, SPD tensors and Manning
roughness all carried by the NetCDF file:

1. Sloping-bed + varying-phi_t lake at rest stays at rest (1e-12, 20 steps)
   after the I/O round trip (Audusse reconstruction + WB pairing survive the
   file path).
2. DischargeInflow volume audit exact across 10 steps:
   total volume gain == sum(boundary_inflow_volume) == q*L*dt*N (1e-12).
3. Authored fields round-trip bitwise.

Manifest: G21 registered (`goldensuite.json` + `check_manifest.py` REQUIRED +
golden root CMake + per-test CMake with `LABELS golden`); manifest
completeness checker passes. G19/G20 are left reserved for the post-G18
coupling completion line to avoid parallel-session ID collision.

## Verification

- Full repo ctest windows-msvc Debug: **126/126 passed** (M255 baseline 123
  + bridge + boundary + G21).
- `check_manifest.py`: OK.
- LNK1168 == 0 on the final build.
