# M287-C4 Coupling Confirmation Contract Evidence

Date: 2026-09-04
Scope: UFM-Mapper master plan v3 §8.2 phase I, slice **C4** — the human
confirmation contract for coupling candidates and the pipeline stage E' that
merges candidates + decisions into the effective links. New gate **G33
`coupling_confirmations_synthetic_case`** (`ci_gate:true`, committed-fixture
pattern like G30/G31/G32). M281 D-5 stays BLOCKED; no runtime semantics touched.

## Deliverables

- `python/scau_preproc/confirmations.py` — contract + merge:
  - `coupling/confirmed/*.json`, `confirmation_schema_version = 1`, one file per
    decision: `chain` (`surface_to_swmm` | `roof_to_swmm`), `mapping_id`,
    `decision` (`accept` | `reject` | `retarget` | `create`), `candidate_sha256`
    (canonical sorted-key compact-JSON hash of the generator's candidate; `null`
    only for `create`), `confirmed_by`, ISO `timestamp`, optional `target`
    (`cell_index`, `exchange_elevation_m`, plus `swmm_node_id` / `building_id`
    for `create`), `note`.
  - Merge semantics: accept → effective as-is; retarget → operator cell/crest,
    `method += "+operator_retarget"`; reject → dropped; create → new
    `operator_created` link; **no decision → not effective, blocks export**.
  - **Drift detection**: hash mismatch ⇒ decision ignored, candidate reverts to
    unconfirmed, `ConfirmationDrift` review finding with the object id. Structural
    errors (schema, unknown chain/decision, unknown mapping id, duplicate
    decisions, cell out of range, create reusing an existing id, hash on create,
    target on accept/reject) are **fatal** `ConfirmationInvalid`.
  - Outputs: `coupling/effective_links.json` (`effective_links_schema_version = 1`,
    status `complete|incomplete`, per-chain counts, provenance hashes of every
    confirmation and candidate file, links with `confirmed_by/confirmed_at/
    candidate_sha256/confirmation_file`) and `coupling/effective/simdriver_links.conf`
    (RuntimeConfig v2, **effective ground links only**; the candidate fragment
    `coupling/simdriver_links.conf` is left untouched).
- `python/scau_preproc/pipeline.py` — stage E' after coupling maps: optional
  `job_config.confirmations_dir` (relative to the job_config; configured-but-missing
  is a contract error), default = package `coupling/confirmed/` when present;
  `validation.coupling_confirmations` report; unconfirmed candidates ⇒ `review`
  finding `CouplingCandidatesUnconfirmed` and **pipeline status `review`** (exit 0,
  export gate closed); confirmation file hashes recorded in `pipeline_manifest.json`.
- `samples/d5_gis_preproc_template/coupling/confirmed/` — synthetic decisions:
  accept J1, retarget J2 → cell 50 / crest 9.6 m, reject roof 1, accept roof 2
  (+ README of the contract).
- `qgis_plugin/scau_preproc_workbench/` — `jobio.exportable` now treats **review**
  like fatal; `confirmation_rows` appended to findings; `build_job_config(...,
  confirmations_dir)`; job page gains a 耦合确认目录 field; export label text names
  the review/unconfirmed cause; `layer_paths` lists `effective_links` (tabular, for E4).
- `tests/golden/coupling_confirmations_synthetic_case/` — G33 fixture (candidates
  byte-identical to G31, four confirmation files, `effective_links.json`, both
  fragments, pipeline manifest) + C++ golden; registered in `goldensuite.json`,
  `check_manifest.py` REQUIRED, per-test CMakeLists `LABELS golden`.
- `python/tests/test_confirmations.py` (7) + jobio tests (2 new) — headless.

## Evidence

1. **Unit** — `py -3 -m unittest discover -s python/tests -t python`: 25/25
   (merge matrix accept/retarget/reject/create, drift, every structural error,
   sorted loading, canonical hashing, export-gate review semantics).
2. **Pipeline, no confirmations** (D-5 package before the fixtures were added):
   status `review`, 1 finding `CouplingCandidatesUnconfirmed` (4), effective
   fragment holds only `version = 2` + comment; case SHA unchanged
   (`a89ba98d…` = G30). Exit 0.
3. **Pipeline, package confirmations**: status `ok`, no findings, chains
   `surface {accepted 1, retargeted 1}`, `roof {accepted 1, rejected 1}`;
   effective fragment `cell=379,node=J1,crest=10.0` / `cell=50,node=J2,crest=9.6`;
   manifest lists the four confirmation SHA-256s.
4. **Drift**: same confirmations against the mesh-controls mesh (cell indices
   shift) → 4 × `ConfirmationDrift` (review) + `CouplingCandidatesUnconfirmed`;
   status `review`, nothing effective. A stale decision with an out-of-range
   target is *ignored* (drift wins), a **matching-hash** out-of-range retarget is
   fatal `ConfirmationInvalid: target.cell_index 99999 outside mesh (cells=469)`,
   exit 2.
5. **G33 golden** (SimDriver parser + surface2d loader as the authoritative
   consumers): candidate vs effective fragments differ exactly by the operator
   decisions; the retargeted cell does **not** contain J2 (operator choice, not
   containment), lies on the road strip within one characteristic length of J2,
   Manning 0.015, crest below its `z_b`; `effective_links.json` has complete
   status, 3 confirmed links, 0 rejected links present, provenance for 4 + 2
   input files. `ctest -R "preproc|coupling_maps|coupling_confirmations|manifest"`
   8/8. During authoring the C++ adjacency check caught a wrong hand-picked
   retarget cell (233 at x≈25 m) — fixture corrected to the true neighbour 50.
6. **QGIS 4.0.0 offscreen smoke** (deployed profile copy): run with package
   confirmations → `PipelineOk` + `CouplingConfirmations pass` + chain rows,
   export gate 允许; run with an empty confirmations dir → `review` rows, export
   gate 禁止（… 未确认耦合候选）.

## Boundaries kept

- Generator output (`*_mapping.json`, candidate `simdriver_links.conf`) is never
  edited; decisions live in versioned input files with author/time/hash.
- No spatial nearest-neighbour becomes authoritative: only explicit decisions
  produce effective links (C5 will add spatial *candidates*, still through C4).
- No `Q_limit` / deficit / arbitration semantics; placeholders pass through
  unchanged and stay recorded as placeholders.

## Follow-ups

- **C5** spatial-candidate mode (v3 §8.2) → **E4** coupling editor writing this
  contract (map highlight, review in red, accept/reject/retarget/create).
- **B6** exporter must consume `effective/simdriver_links.conf` and refuse
  `status: incomplete`.
- SimDriver-side refusal of unconfirmed candidates is expressed by the pipeline
  (effective fragment only contains confirmed links); a driver-level guard on
  the candidate fragment is not needed because it is never handed to the driver.
