# D-Flow FM BMI 1.0 variable inventory

Status: template only. Fill this file from a real `get_var_count` /
`get_var_name` / `get_var_type` / `get_var_rank` / `get_var_shape` spike run.

## Purpose

This inventory is the authoritative place to record the real BMI variable names
exposed by the target D-Flow FM build used for G11 `dflowfm_river_steady`.

It should answer the G11 questions that fake/mock coverage cannot answer:

- which variable names map to water level, discharge, and lateral discharge;
- whether the variables are scalar or array-valued;
- their type strings, ranks, shapes, and units;
- whether write paths need `set_var` or `set_var_slice`.

## Capture instructions

The spike host now prints a markdown inventory table directly from
`get_var_count` / `get_var_name` / `get_var_type` / `get_var_rank` /
`get_var_shape` / `get_var_units`. During a real runtime pass, copy that table
output into this file and then fill the `read/write role` and `notes` columns.

For each exported variable observed during the real spike run, record:

| index | name | type | rank | shape | units | read/write role | notes |
|---|---|---|---|---|---|---|---|
| 0 | TBD | TBD | TBD | TBD | TBD | TBD | TBD |

## Required highlights for G11

At minimum, identify and document the actual runtime names for:

- water level / stage;
- discharge / flow;
- lateral discharge;
- any weir / gate / structure variables needed by the `reach_with_weir` case.

## Follow-on usage

- `spike_report.md` should summarize the conclusions from this inventory.
- `interface_gap_matrix.md` should downgrade `TBD` / `MATCH_WITH_NOTE` entries
  only after this file has real data.
- `DFlowFMEngine` should not widen its public assumptions until the relevant
  runtime names and shapes have been captured here.
