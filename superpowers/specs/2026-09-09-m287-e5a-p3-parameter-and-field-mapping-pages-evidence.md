# M287-E5a (P5 参数表) + P3 (字段映射) Evidence

Date: 2026-09-09
Scope: UFM-Mapper master plan v3 §5 pages **P5** (semantic / physical parameter
tables) and **P3** (source field → canonical target mapping). Both are pure
renderings + versioned writers in `qgis_plugin/scau_preproc_workbench/jobio.py`
(no qgis imports; CPython-testable); the dialog only binds tables and buttons.
No pipeline behaviour changes; no new gate (UI slice, plan v3 §8.3 "离屏冒烟").

## Deliverables

### P5 参数表 (E5a)
- Browse `metadata/dpm_rule_table.json` as two tables (classes: `phi_t`,
  `Phi_c.xx/xy/yy`, note; interfaces: class pair → `omega_edge`) and
  `soil/soil_parameters.csv` as one table.
- `lint_rule_table` mirrors the closure laws the pipeline enforces
  (`phi_t ∈ (0,1]`, `Phi_c` SPD with `λ ∈ [epsilon_phi, 1]`, `cond ≤ cond_max`,
  `phi_t ≥ max(xx, yy)`, interface classes known, `omega ∈ [0,1]`) using the
  table's own `closure_limits`; a table the lint rejects is also rejected by
  `field_derivation.load_rule_table` (asserted). `lint_soil_rows` checks
  `K_s ≥ 0`, `psi_f > 0`, `θ_s ∈ (0,1]`, `0 ≤ θ_i < θ_s`, contiguous
  `soil_type = 0..n-1` (STCF `soil_type_entry` index).
- **Editing never rewrites the source**: `write_rule_table_version` /
  `write_soil_table_version` write `<stem>.vNNN.<ext>` (next free number in the
  family, `.tmp` + atomic replace). An edited rule table carries a `revision`
  block (`based_on`, `based_on_sha256`, `edited_by`, timestamp, note) and its
  `approval` is **reset to `synthetic_unapproved`** — an edit invalidates the
  data owner's approval (M281); the pipeline refuses it for a non-synthetic
  package until re-approved. Lint-fatal tables are refused (nothing written).
- Dialog page "参数表": load/refresh, lint, add/remove rows, save rule / soil
  versions; finding rows tell the operator to point the job page's
  "字段派生规则表" at the new version.

### P3 字段映射
- `discover_source_fields`: GeoJSON property names (first non-null sample and
  type), CSV headers, DEM = `elevation`, per manifest dataset.
- Targets offered: `manifest.canonical_targets` (every entry must be in the
  pipeline's `CANONICAL_TARGETS`, pinned by test) or the full canonical set;
  plus `(unmapped)`. Nothing else can be selected.
- Seeding (no saved mapping): `metadata/field_dictionary.csv` rows with a
  canonical `target_field` (an explicit `N/A` pins `(unmapped)`), then a
  same-name default **only for spatial datasets** (LUT columns such as
  `soil_parameters.soil_type` are never cell fields). A canonical target has
  exactly one source: the first dataset (manifest order) keeps the seed, later
  ones are `(unmapped)` with a `FieldMappingAmbiguousSeed` review row
  (sample: `landcover.soil_type` vs `soil_zones.soil_type`).
- `write_field_mapping_version` → `metadata/field_mapping.vNNN.json`
  (`field_mapping_schema_version 1`, `status: declaration_only`); rejects
  non-canonical targets and duplicate targets. A saved mapping (latest version)
  is authoritative on reload: fields it does not list are unmapped.
- Dialog page "字段映射": discovery table with a per-row `QComboBox` restricted
  to the allowed targets; save writes the versioned file.

### Tooling
- `tools/qgis/offscreen_smoke.py --exercise-edits`: on a COPY of the package,
  edits a legal cell, saves rule / soil / mapping versions through the dialog
  slots and asserts exactly `dpm_rule_table.v001.json`,
  `soil_parameters.v001.csv`, `field_mapping.v001.json` appear.
- `python/tests/test_jobio_parameter_tables.py` (8) → 68 headless tests on this
  branch (73 once B3 lands).
- Plugin 0.10.0.

## Evidence

1. Sample rule table: 3 classes / 1 interface render; lint `pass`, approval
   `review (synthetic_unapproved)`; rows → table round trip is lossless.
2. Lint ↔ pipeline agreement: `road.phi_t = 0.5` → `RuleStorageBelowConveyance`
   in the UI **and** `FieldDerivationError` in `field_derivation.load_rule_table`.
   Also covered: non-SPD tensor, `λ_max > 1`, `phi_t = 0`, unknown interface
   class, `omega_edge = 1.5`, empty class code.
3. Versioned write: source bytes unchanged; `v001` then `v002` (based on
   `v001`); approval reset from `approved` → `synthetic_unapproved`; the new
   version loads through the pipeline's own loader for a synthetic package;
   a lint-fatal edit writes nothing.
4. Soil: edit `K_s` → `soil_parameters.v001.csv`, source untouched,
   `source_or_authority = edited_by:… (re-approval required)`; `θ_i ≥ θ_s` and
   non-contiguous `soil_type` rejected.
5. Field mapping on the sample: `dem.elevation → z_b`,
   `landcover.manning_n → manning_n`, `landcover.soil_type → soil_type`,
   `soil_zones.soil_type → (unmapped)` + ambiguous-seed review row,
   `buildings.building_id → (unmapped)`; every target canonical or unmapped;
   coverage `pass`. Write rejects `roughness` and a second source for `z_b`;
   operator moving soil to `soil_zones` writes `field_mapping.v001.json` and
   reload honours it; a manifest declaring a non-canonical target is refused.
6. **QGIS 4.0.0 offscreen** (`offscreen_smoke.py --exercise-edits` on a package
   copy): construct, data tree, report browser, parameter tables, field
   mapping all ok; edits produce exactly the three `v001` files; the edited
   rule table has `grass.Phi_c.yy = 0.9`, `approval.status =
   synthetic_unapproved`, `revision.edited_by = smoke`. The first smoke attempt
   with an illegal edit (`grass.phi_t = 0.99 < Phi_c.xx`) was correctly refused
   by the dialog (`RuleTableRejected: RuleStorageBelowConveyance`).

## Boundaries kept

- Source tables are never modified in place; the pipeline keeps reading the
  path the job config names (the operator selects a version explicitly).
- Parameter *values* remain a governance input: any edit drops approval.
- P3 is declaration-only until M281 unlocks the real importer (M287-D); it
  cannot change what the synthetic pipeline computes.

## Follow-ups

- `soil_lut.json` vs `soil_parameters.csv` coexistence (v3 §4.2) still needs a
  ruling before the pipeline consumes edited soil versions directly.
- Wire `field_mapping.vNNN.json` into the M287-D importer contract once M281
  provides the authorized sample.
