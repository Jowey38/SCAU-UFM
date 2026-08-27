# M287-B PreProc Pipeline v1 Evidence

Date: 2026-08-27
Scope: M287 plan phase B — configuration-driven preprocessing pipeline over
the synthetic D-5 package, its committed deterministic output fixture, and
the new gated golden G30. Synthetic inputs only; M281 D-5 stays BLOCKED.

## Deliverables

- `python/scau_preproc/` (pipeline v1):
  - `meshgen.py` — production port of the M287-A generator (unchanged
    arithmetic; subprocess-only gmsh, GPL boundary preserved, rejection-only
    geometry pre-checks, atomic output);
  - `pipeline.py` — stage A-D orchestration:
    `job_config.json` (schema v1) -> package contract checks (required
    files, canonical mapping targets, `geometry_clean_policy` guards
    incl. `partial_mesh_output: forbidden` and `unapproved_repair: fatal`)
    -> policy-timeout generator subprocess -> optional bitwise rerun check
    -> authoritative `scau_preproc validate` -> `pipeline_manifest.json`
    (source/config/case SHA-256, quality, reproduce command) +
    `validation.json` (status + findings). Exit codes: 0 ok / 2 fail-closed
    / 3 timeout. This is the exact stateless entry point the future QGIS UI
    must drive (M287 plan section 3).
- Committed fixture `tests/golden/preproc_gis_synthetic_case/cases/`:
  `synthetic_city_block.stcf.nc` (90,120 bytes, SHA-256
  `a89ba98da95bcd5732c17e1bf9977e8f75bdc1d81b96a6844333fc796e911bc4`) plus
  its pipeline manifest.
- **G30 `preproc_gis_synthetic_case`** (`ci_gate: true`): strict-loads the
  committed case through `load_surface2d_case` and asserts the recorded
  shape (526 nodes / 469 faces / 996 edges), field domains (unit phi_t /
  omega / phi_e_n placeholders, DEM-sampled z_b in [10.0, 10.9], landcover
  Manning set), zero fatal/review mesh-quality findings, and a 20-step
  wall-closed lake at rest over the sampled DEM bed within 1e-12.

## Runs (local, 2026-08-27)

| Check | Result |
|---|---|
| pipeline nominal (`determinism_check: true`) | status ok; bitwise_identical_reruns true; authoritative validate exit 0 |
| cross-checkout determinism | pipeline case SHA-256 equals the M287-A spike case hash exactly (independent worktree + output dir) |
| fail-closed: package missing `geometry_clean_policy.json` | exit 2 before any meshing |
| fail-closed: degenerate (bowtie) building | exit 2, `MeshGenerationFailed` finding with diagnostic GeoJSON path, no partial output |
| G30 golden | 3/3 assertions green (shape/domains, quality, lake-at-rest) |
| manifest checker | OK with G30 registered (entry + REQUIRED + unquoted `LABELS golden`) |
| full suite | see PR record (local ctest run) |

## M287-B exit criteria status

| Criterion | Status |
|---|---|
| A-D 阶段 CLI 化（合成包 → 完整 STCF 案例） | DONE (pipeline v1; DPM 字段仍为记录在案的占位规则，真实派生属后续切片) |
| 新 golden `preproc_gis_synthetic_case`（同平台确定性重生成逐位比较 + 写读 roundtrip bitwise） | DONE with a governed split: bitwise rerun + write-read certification run pipeline-side (gmsh required); the hosted-CI golden locks the committed deterministic fixture through the authoritative read path. |
| geometry_clean_policy 驱动的修复审计报告 | DONE (policy audit in `validation.json`; v1 mode is rejection-only, zero repairs by construction) |
| 双平台哈希对比实验 | OPEN — hosted Linux lanes have no gmsh; the cross-platform hash experiment needs a governed Linux gmsh environment. Cross-platform gate form stays undecided per plan v1.1 (evidence-gated). |

## Boundaries

- No real GIS importer, no real-data claims; landcover/soil/DPM rules remain
  synthetic placeholders recorded in the case manifest.
- No runtime coupling semantics in preprocessing outputs.
- gmsh remains outside the main CMake graph; the golden intentionally does
  not invoke regeneration.
