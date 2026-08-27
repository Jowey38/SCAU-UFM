# M287-A Gmsh Mesh-Generation Spike Evidence

Date: 2026-08-27
Scope: M287 plan phase A — governed mesh-generation spike over the synthetic
D-5 input package. Spike-only plus one production addition
(`scau_preproc validate`); no solver change; M281 D-5 stays BLOCKED
(all inputs are the authored synthetic template, not real city data).

## Deliverables

- `spikes/gmsh_meshgen/` — subprocess-isolated generator
  (GeoJSON boundary + building holes -> gmsh 4.15.2 -> CCW faces -> derived
  UGRID edge/edge_faces/face_edges -> uniform-DPM STCF v5 NetCDF -> quality
  report), runner with three scenarios, degenerate fixture, evidence
  artifacts.
- `scau_preproc validate --input <case.stcf.nc>` — authoritative external
  certification path: loads through `read_stcf_case` (full dataset +
  topology validation, the same rules the solver bridge enforces). CLI unit
  test extended (valid case passes; missing file and missing `--input`
  fail).
- Governance: `third_party/manifest/gmsh.version` (PyPI 4.15.2 pin,
  spike-only status, determinism pinning) and
  `third_party/licenses/gmsh-LICENSE-NOTE.md` (GPL-2.0+; subprocess
  file-interchange boundary is the license boundary; in-process embedding
  requires a fresh review).
- Sample-package fix: `soil_parameters.csv` entry 0 `theta_i` 0.1 -> 0.05
  (validator requires `theta_i < theta_s`).

## Results (local, 2026-08-27; artifacts under `spikes/gmsh_meshgen/evidence/`)

| Scenario | Outcome |
|---|---|
| nominal (lc=8 m, Blossom recombine) | 526 nodes / 469 faces (469 quad, 0 tri) / 996 edges; min angle 43.64 deg, max edge-length ratio 2.36 (review thresholds 10 deg / 20) |
| determinism | two independent runs bitwise identical (`.stcf.nc` SHA-256 equal) |
| degenerate (bowtie building fixture) | rejected BEFORE meshing: exit 2, diagnostic GeoJSON names `BLDG_DEGENERATE_BOWTIE`, no output and no `.tmp` left |
| timeout (lc=0.08 m under 20 s policy override) | subprocess killed at the wall-clock limit; `MeshGenerationFailed` recorded; no partial output |
| authoritative certification | `scau_preproc validate` exit 0 on the spike case (nodes=526, faces=469, edges=996, soil_entries=2) |

Full suite with the CLI change: see the PR CI lanes (hosted + self-hosted)
for the recorded runs; local ctest run recorded in the PR description.

## M287-A exit criteria

| Criterion | Status |
|---|---|
| 剖分结果过 `validate_stcf_case` + quality 报告 | DONE (authoritative `validate` exit 0; quality JSON archived) |
| 第三方治理文件齐备 | DONE (manifest + license note; spike-only, subprocess isolation) |
| 超时/退化几何 fail-closed 证据（死锁夹具 + 诊断 GeoJSON） | DONE (both scenarios archived, no-partial-output verified) |

## Boundaries and follow-ups

- Field values beyond the mesh (uniform `phi_t`/tensor/edge fields, nearest
  DEM sampling, landcover lookup) are spike placeholders; M287-B owns real
  field derivation and the `preproc_gis_synthetic_case` golden.
- The Python-side geometry pre-checks implement `geometry_clean_policy`
  rejection only (no repairs); they are input hygiene, not a second
  validator — certification remains exclusively `scau_preproc validate`.
- gmsh stays out of the main CMake graph; production adoption keeps the
  subprocess boundary or completes a recorded license review first.
