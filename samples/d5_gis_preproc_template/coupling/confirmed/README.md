# coupling/confirmed/ — operator confirmations (M287-C4 contract, schema 1)

One JSON file per decision. Decisions: `accept`, `reject`, `retarget` (with `target.cell_index`
and optional `target.exchange_elevation_m`), `create` (no candidate; `target` carries
`swmm_node_id`, `cell_index`, `exchange_elevation_m`, plus `building_id` for roofs).
`candidate_sha256` is the canonical hash of the generator's candidate relation; when the
candidate changes underneath the decision, the pipeline reports `ConfirmationDrift` and the
candidate reverts to unconfirmed. These fixtures are synthetic; a real operator identity and
review record replace them when M281 unblocks.
