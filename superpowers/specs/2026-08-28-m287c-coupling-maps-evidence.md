# M287-C Coupling-Map Generator Evidence

Date: 2026-08-28
Scope: M287 plan phase C — candidate coupling-relationship generation for the
three chains over the synthetic D-5 package, with SimDriver-native
consumption proof (G31). Synthetic inputs only; M281 D-5 stays BLOCKED.

## Deliverables

- `python/scau_preproc/coupling_maps.py` (schema
  `coupling_mapping_schema_version = 1`):
  - **surface -> SWMM**: explicit CSV IDs are authoritative; the generator
    resolves each SWMM node coordinate to its containing mesh cell by ray
    casting. A node outside every cell is FATAL — no silent
    nearest-neighbor fallback for the ground chain.
  - **roof -> SWMM**: explicit drain/node IDs preserved; the overflow target
    cell is a nearest-centroid CANDIDATE (buildings are mesh holes), always
    `confidence: review` + `review_status: needs_confirmation`.
  - **surface -> D-Flow FM**: emitted as `provider_required` with zero
    relations (template rows are TODO_PROVIDER; nothing is invented).
  - Outputs: 3 mapping JSONs + `mapping_report.json` +
    `simdriver_links.conf` (SimDriver-native `version = 2` fragment with one
    `surface_drainage_link = cell=..,node=..,crest=..,width=..,weight=..`
    per ground relation; width/weight are recorded v1 placeholders).
  - `pipeline.py` stage E: `"coupling_maps": true` runs the generator inside
    the M287-B pipeline; failures are fatal findings in `validation.json`.
- Synthetic-template consistency fix surfaced BY the fail-closed path:
  the SWMM junctions previously sat at building centroids (mesh holes);
  the generator refused (exit 2). `[COORDINATES]` moved onto the road strip
  (J1 55,10 / J2 135,10 / O1 190,10), matching physical street inlets.
- **G31 `coupling_maps_synthetic_case`** (`ci_gate: true`): parses the
  committed fragment through the REAL `apps/sim_driver` runtime-config
  parser (2 links: cell 379 -> J1 crest 10.0, cell 234 -> J2 crest 9.5;
  no river/drainage-river links), then independently re-verifies the
  python containment in C++ against the committed G30 mesh fixture
  (point-in-cell per node) and roof target validity.

## Runs (local, 2026-08-28)

| Check | Result |
|---|---|
| generator on synthetic package | 2 surface + 2 roof relations, dflowfm provider_required, exit 0 |
| fail-closed (node inside a building hole, pre-fix) | exit 2 with the offending node named |
| determinism | independent reruns byte-identical (all 5 artifacts) |
| pipeline stage E end-to-end | status ok; artifacts byte-identical to the committed fixtures |
| G31 golden | 2/2 tests green (SimDriver parse + C++ geometry re-verification) |
| manifest checker | OK with G31 registered |
| full suite | 166 tests expected green (recorded in the PR) |

## M287-C exit criteria

| Criterion | Status |
|---|---|
| 三条链路候选 + 确认文件格式 + 映射报告 | DONE（低置信度关系强制 review 标记；D-Flow FM 链路按 fail-closed 出 provider_required） |
| 新 golden：映射文件 schema 校验 + 合成包端到端 | DONE（schema 校验 python 侧；端到端走 pipeline 阶段 E） |
| 映射文件被 SimDriver 消费通过 | DONE（G31 用 SimDriver 自己的 `read_runtime_config_file` 解析生成片段并锁定字段值） |

## Boundaries

- Generator emits data/candidates/config fragments only — `Q_limit`,
  `V_limit`, deficit, rollback/replay, and arbitration stay CouplingLib-owned.
- Roof-chain SimDriver consumption is NOT claimed: run_loop has no roof
  config key today; the roof mapping JSON is the reviewed hand-off for the
  future RoofSwmmStepDriver wiring milestone.
- exchange_width/priority_weight are recorded placeholders pending provider
  confirmation; real hydraulic values enter with the D-5 contract.
