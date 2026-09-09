# Sample package self-check (A6)

Run before trusting a green pipeline result on ANY package (synthetic or real).
Two defects shipped in the synthetic D-5 template were only caught late:

| Defect | Why the pipeline did not see it | Where it was caught |
|---|---|---|
| `swmm/model.inp` carried a non-SWMM `[END]` section | Python read only `[COORDINATES]` | real SWMM 5.2.4 cold start in the B6 golden (ERROR 205) |
| landcover pond polygon inside the grass polygon | v1 first-match roughness lookup silently chose grass for 23 cells | B5 field derivation (`landcover_overlap` rule) |

## Automated pre-flight

```text
py -3 -m scau_preproc.package_selfcheck <package_dir> [--json selfcheck.json]
```

Exit 0 = no fatal finding (status `ok` or `review`); exit 2 = fatal. Findings use
the pipeline `{severity, code, detail, objects[]}` shape. Checks:

- [ ] `ManifestRequiredMissing` / `FileNotInManifest` — every file is a declared dataset (or a known pipeline/UI side file)
- [ ] `PolicyAbsent` / `PolicyUnversioned` — `geometry_clean_policy` (fatal if absent), `crs_policy`, `terrain_condition_policy`, `dpm_rule_table` carry their `*_schema_version`
- [ ] `SwmmUnknownSection` — every `[SECTION]` is in the SWMM 5.2 list (real engine parity)
- [ ] `SwmmRunoffSectionPresent` — no `[SUBCATCHMENTS]/[SUBAREAS]/[INFILTRATION]/[RAINGAGES]` (runoff is Surface2D-owned, M247)
- [ ] `SwmmNodeWithoutCoordinates` — nodes have coordinates (spatial candidates need them)
- [ ] `MappingNodeUnknown` — mapping tables reference nodes the `.inp` defines
- [ ] `LandcoverOverlap` — overlapping class polygons are fatal unless `dpm_rule_table.landcover_overlap = smallest_area_wins` (then review)
- [ ] `DemDoesNotCoverBoundary` / `DemNoDataInsideBoundary` / `BoundaryDegenerate`
- [ ] `PlaceholderInRealPackage` — no `TODO_PROVIDER` when `synthetic_data: false`

## Manual items the script cannot decide

- [ ] `.inp` cold-starts in the real embedded engine (`scau_sim` / G35 pattern) — the self-check only knows section names
- [ ] DEM interpretation (bare earth vs DSM) and vertical datum are confirmed
- [ ] Every DPM rule-table value has data-owner approval (`approval.status = approved`) for a real package
- [ ] CRS policy `target_crs` is the project CRS, not the template identity frame
- [ ] `terrain_condition_policy.enabled` is `true` only with a filled `authorization` block
- [ ] Pipeline determinism check passes (`determinism.bitwise_identical_reruns = true`) and `scau_preproc validate` is `ok`

Current template result (2026-09-09): `status: review` — the single review item is
the declared pond-in-grass overlap.
