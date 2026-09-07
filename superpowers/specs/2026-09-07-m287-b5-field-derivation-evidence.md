# M287-B5 Rule-Table Field Derivation Evidence (format first)

Date: 2026-09-07
Scope: UFM-Mapper master plan v3 §8.3 slice **B5** — physical field derivation
(stage D) driven by a versioned, approval-gated rule table. Format and checks
land now; real parameter *values* remain a data-owner input (M281 / v1.1 §4 red
line). New gate **G36 `preproc_field_derivation_case`** (`ci_gate:true`).

## Deliverables

- `metadata/dpm_rule_table.json` contract (`dpm_rule_table_schema_version 1`):
  `approval {status: approved | synthetic_unapproved, approved_by, date}`,
  `closure_limits`, `classes.<landcover class_code> {phi_t, Phi_c{xx,xy,yy}}`,
  `unmapped_class: fatal | default` (+ `default_class` incl. `manning_n`),
  `landcover_overlap: fatal | smallest_area_wins`, `edges {default_omega,
  boundary_omega, interfaces[{classes[a,b], omega_edge}]}`, `soil {source:
  soil_zones | landcover, missing_zone: fatal | {soil_type}}`.
- `python/scau_preproc/field_derivation.py`:
  - **Lint** (fail-closed, entry-addressed): every class tuple must satisfy the
    C++ closure laws — `phi_t ∈ (0,1]`, `Phi_c` SPD, `λ_min ≥ ε_φ`, `λ_max ≤ 1`,
    `cond ≤ cond_max`, `phi_t ≥ max diag`; interfaces must reference known
    classes; ω ∈ [0,1]; unapproved table + non-synthetic package ⇒ fatal;
    approved table needs `approved_by` + `date`.
  - **Cells**: class from the landcover polygon containing the centroid;
    overlapping classes fatal unless `smallest_area_wins`; `phi_t`, `Phi_c`,
    `manning_n` (same polygon ⇒ mutually consistent), `soil_type` from
    `soil_zones` (or landcover); unmapped/missing rules fatal or defaulted per
    table. Nothing is inferred from geometry.
  - **Edges** = main-spec 5.3 rule 2 (M244/M245): `Φ_e = ½(Φ_L+Φ_R)`
    (boundary: inside cell), `phi_e_n = ω·nᵀΦ_e n`, `phi_et = ω·tᵀΦ_e t`,
    ω from the class-pair interface table. Computed in the algebraically
    identical unit-vector form `half_trace + half_diff·(nx²−ny²) + 2xy·nx·ny`.
  - `field_derivation.json` report (approval, census, ranges, overlap count,
    interface edges, rule text, rule-table SHA-256).
- `meshgen.py` / `pipeline.py`: `job_config.field_derivation {dpm_rule_table,
  soil_zones?}` → generator config; **without the block the v1 placeholder path
  is byte-identical** (G30 SHA `a89ba98d…` reproduced); finding code
  `FieldDerivationFailed`; rule-table SHA in `pipeline_manifest.json`.
- Sample `metadata/dpm_rule_table.json` (synthetic_unapproved: road 0.95 /
  diag(0.95, 0.80), grass & water unit isotropic, kerb road|grass ω 0.9,
  soil from zones with road strip → type 0, smallest-area overlap).
- QGIS plugin 0.8.0: 规则表 field on the job page; 字段 (D) report page renders the
  derivation report.
- `python/tests/test_field_derivation.py` (11) → 55 headless tests total.
- G36 fixture (`synthetic_city_block_derived.stcf.nc`, SHA `1c762869…`, report,
  manifest, rule table) + C++ golden; manifest/checker/CMake 3-way sync.

## Evidence

1. **Placeholder path unchanged**: pipeline without `field_derivation` →
   `a89ba98d…` (= G30), so G30–G35 fixtures are untouched.
2. **Derived run** (D-5 package): status ok, `scau_preproc validate` exit 0,
   bitwise rerun; census road 83 / grass 363 / **water 23**; 30 kerb edges at
   ω = 0.9; `phi_e_n ∈ [0.8, 1.0]`.
3. **Negatives**: unapproved table on a package with `synthetic_data: false` ⇒
   fatal (M281 message); `phi_t` below max diagonal ⇒ fatal naming
   `classes.road`; `missing_zone: fatal` ⇒ fatal listing 83 road cells;
   overlapping landcover without the opt-in ⇒ fatal naming the centroid and
   classes.
4. **G36 golden** (authoritative loader + solver kernels): every cell's
   (phi_t, Φ_c, manning, soil) is exactly a rule-table tuple with the recorded
   census; every edge's `phi_e_n` equals the solver's own
   `project_edge_conveyance` to 1e-12 and respects `≤ 1`; kerb ω 0.9 on exactly
   30 edges; `validate_dpm_cell_consistency` passes on all 469 cells; 20-step
   lake at rest ≤ 1e-12 with the spatial φ_t jump and anisotropic Φ_c.
   `ctest -R "preproc|coupling…|case_export|manifest"` 11/11.
5. **QGIS 4.0.0 offscreen**: rule-table field → `job_config.field_derivation`
   → run ok → 字段 page shows `rule-table derivation (B5)`, approval, census with
   water, 30 interface edges, overlap rule.

## Findings

- **Round-off vs strict validator**: the naive quadratic form `xx·nx² + 2xy·nx·ny
  + yy·ny²` yields `1.0000000000000002` on some oblique isotropic edges and the
  file validator's strict `phi_e_n ≤ 1` rejected the case (caught by
  `scau_preproc validate`, exactly as designed). The solver's own
  `quadratic_form` uses the same naive expression, so a solver-side
  `assemble_edge_conveyance_from_tensors` could produce the same 1+ε — a latent
  seam between `libs/stcf` strictness and `libs/surface2d` assembly, recorded for
  the kernel line. The writer now uses the unit-vector form (exactly 1.0 for
  isotropic tensors); G36 pins writer/kernel agreement to 1e-12.
- **Overlapping sample landcover**: the pond polygon lies inside the grass
  polygon; v1 first-match never assigned `water` (23 cells silently grass,
  Manning 0.030 instead of 0.020). Placeholder mode keeps that behaviour (G30
  bytes); derived mode makes the rule explicit (`landcover_overlap`, default
  fatal) and the sample opts into smallest-area. The v1 behaviour is now a
  documented limitation of placeholder mode, not a hidden one.

## Decisions

- **G30 is not tightened**; G36 is the derived-field gate. Regenerating the G30
  fixture would cascade into G31/G33/G34/G35 (all reference its mesh/SHA); the
  plan's "tighten G30 placeholder assertions" is satisfied by G36 asserting the
  rule-table tuples on the same mesh while G30 continues to document v1
  placeholder mode.
- Soil parameters stay in `soil/soil_parameters.csv` (plan v3 §12 default); the
  table only decides *which* soil_type a cell gets.

## Follow-ups

- **M287-D**: replace `synthetic_unapproved` with an approved table (values are
  the only thing that changes).
- **E5a** parameter-table page (browse/version rule tables) — UI only.
- Kernel line: decide whether `libs/surface2d` `quadratic_form` should adopt the
  round-off-safe form or the validator should tolerate ≤ 4 ulp above 1.
