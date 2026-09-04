# M287-B6 Case Package Exporter Evidence

Date: 2026-09-05
Scope: UFM-Mapper master plan v3 §8.4 slice **B6** (pipeline stage F) plus the
minimal E5 surface (export button behind the gate). New gate **G35
`case_export_synthetic_package`** (`ci_gate:true`; committed exported package as
fixture). M281 D-5 stays BLOCKED; no runtime semantics touched.

## Deliverables

- `python/scau_preproc/case_export.py`:
  - **Gate** (`check_export_gate`): `validation.status == ok`, no fatal/review
    findings, coupling stage ran, stage E' ran with `effective_links.status ==
    complete`, case file present with SHA-256 equal to both validation and the
    pipeline manifest. Every blocker is listed in one `ExportError`.
  - **Assembly** into a staging dir → single atomic rename: `mesh/` (case + quality
    reports), `coupling/` (candidates, `mapping_report`, `effective_links.json`,
    `confirmed/*`), `swmm/model.inp` **verbatim**, `dflowfm/**` verbatim,
    `metadata/` (geometry policy, package manifest, mesh controls), `validation/`
    (validation, pipeline manifest, job_config), `simdriver/run.conf`.
  - **`simdriver/run.conf`** = RuntimeConfig v2, package-relative paths, timing /
    `engine_mode` / `initial_eta` from `job_config.export_case` (defaults recorded;
    `initial_eta` defaults to DEM max + 0.5 m, source recorded), `output_summary_path`,
    and **only effective ground links** (never the candidate fragment).
    `enable_dflowfm = false` (no `dflowfm_mdu_path`) while the river chain is
    `provider_required`.
  - `manifest.json` (`package_manifest_schema_version 1`): per-file SHA-256 table,
    source hashes (job_config, case, package manifest, policy, mesh controls,
    confirmation files), gate evidence, run_conf record, reproduce commands,
    staged-case certification (`scau_preproc validate` on the staged copy when a
    validator is given; failure ⇒ nothing exported). No timestamps ⇒ deterministic.
  - Target protection: existing target refused unless `force`; target inside the
    pipeline output dir refused; staging always cleaned on failure.
  - CLI `py -3 -m scau_preproc.case_export <output_dir> <target> [--force]
    [--validator=…] [--engine-mode=…]`; `package_hash()` = digest over the file table.
- `pipeline.py` stage F: `job_config.export_case = {target_dir, force?, timing…,
  engine_mode?, initial_eta?}`; blocked export ⇒ fatal `CaseExportBlocked`
  (exit 2); success ⇒ `validation.case_export {target_dir, package_hash, files}`.
- QGIS plugin 0.6.0: 导出案例包 button enabled only when `jobio.exportable`;
  target field; export re-runs the pipeline with `export_case` so the exporter's
  gate decides; `CaseExported` row.
- `samples/d5_gis_preproc_template/swmm/model.inp`: **`[END]` section removed**
  (see finding below).
- `tests/golden/case_export_synthetic_package/` — G35 fixture = exported package
  (`engine_mode=mock`, `end_time=180`) + C++ golden; manifest/checker/CMake 3-way
  sync. `python/tests/test_case_export.py` (6) → 47 headless tests total.

## Evidence

1. **Unit**: gate blocker listing (`status 'review'`, review finding, `2 unconfirmed`),
   coupling-not-run, case hash mismatch; complete file set; `run.conf` content
   (link line, `initial_eta = 11.4`, relative paths, `enable_dflowfm = false`);
   no staging leftovers; **two exports of the same output → identical
   `package_hash`**; existing target refused / force; target-inside-output refused;
   option validation and overrides; failed certification leaves nothing behind.
2. **Pipeline-integrated**: D-5 package with the four sample confirmations →
   `ok`, `case_export {22 files, package_hash 9686d41d…}`; same job with an empty
   confirmations dir → `fatal CaseExportBlocked: … status is 'review'; review
   finding CouplingCandidatesUnconfirmed; effective links incomplete: 4 unconfirmed`,
   exit 2, no target directory created.
3. **Determinism**: two CLI exports of one output dir → `manifest.files` identical;
   only the `reproduce` target path and the validator record differ by construction.
4. **G35 golden** (SimDriver parser + surface2d loader + real embedded SWMM 5.2.4):
   manifest schema/gate fields and per-file entries present, mesh hash = G30
   fixture (`a89ba98d…`); `run.conf` parses with exactly the effective links
   (J1 → 379 crest 10.0; J2 → **50** crest 9.6 — the operator retarget, not the
   candidate 234), river/interface links empty, `enable_dflowfm=false`;
   **cold start**: `SimDriver::configure` accepts the exported config, the case
   loads (469 cells), every crest is below `initial_eta`, `SwmmEngine::initialize`
   opens the shipped `swmm/model.inp` and resolves J1/J2. `ctest -R
   "preproc|coupling…|case_export|manifest"` 10/10 (after clearing an LNK1168
   stale-exe lock and rebuilding — the first "pass" ran the old binary).
5. **QGIS 4.0.0 offscreen**: gate closed → button disabled; gate open → button
   enabled → export → `CaseExported 22 files`, `run.conf` 2 links.

## Findings (logged in `.wolf/buglog.json`)

- **Sample `swmm/model.inp` was not consumable by the real engine**: SWMM 5.2.4
  rejected it with `ERROR 205: invalid keyword at line 55: [END]`. M287-C only ever
  parsed `[COORDINATES]` in Python, so the defect stayed hidden until B6 made
  "cold start with the real engine" a gate. Removed the section; the N-series
  whitelist no longer lists `END`. This is precisely the class of error the N1
  "write → real engine parse" rule is designed to catch.
- **SimDriver run loop is tri-model only**: `run_simulation` throws
  `invalid_argument` for a surface+SWMM-only package although `configure()`
  accepts it. The exporter does **not** fake a river link; G35 locks the
  limitation (`EXPECT_THROW`) so it flips only with C3 river links or a
  driver dual-model mode, with evidence.

## Boundaries kept

- Exporter decides nothing physical: links/crests from `effective_links.json`,
  timing from declared job_config values; `.inp` and D-Flow files copied verbatim.
- Certification is the C++ validator on the staged copy; Python only assembles.
- Placeholders (`exchange_width_m`, `priority_weight`) pass through untouched.

## Follow-ups

- **Driver dual-model mode or C3**: needed before an exported package can be *run*
  end-to-end (M287-F prerequisite; decision for the CouplingLib/driver line).
- **E5 full page**: report browser + package hash display (this slice delivers the
  gate-bound export action only).
- Real-mode `run.conf` (`engine_mode = real`) additionally needs the D-Flow FM
  runtime environment; the exporter records the mode, it does not check the host.
