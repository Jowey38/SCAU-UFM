# D-Flow FM BMI spike runbook

Purpose: produce a reproducible first-pass G11 evidence bundle once a real
D-Flow FM runtime library and an authored `.mdu` case are available.

This runbook does NOT claim that the runtime is currently available in the
repository. It standardizes how to capture evidence when it becomes available.

## Inputs required before running

1. A local Delft3D / D-Flow FM build that exports the BMI library used by
   `spikes/dflowfm/CMakeLists.txt`.
2. An authored `.mdu` case under `spikes/dflowfm/cases/` such as:
   - `single_reach.mdu`
   - `reach_with_weir.mdu`
3. A writable evidence output directory.

## Build the spike host

Example:

```bash
cmake -S spikes/dflowfm -B build/dflowfm-spike -DDFLOWFM_SOURCE_DIR=/path/to/Delft3D-main
cmake --build build/dflowfm-spike
```

## Recommended capture command

Run the host with explicit step count and output files.

For the first sanity pass, prefer the repository-owned `single_reach.mdu`
skeleton:

```bash
build/dflowfm-spike/dflowfm_spike_host spikes/dflowfm/cases/single_reach.mdu \
  --steps 100 \
  --dt 60 \
  --boundary-var boundary_discharge \
  --stage-var stage_at_section \
  --inventory-out spikes/dflowfm/evidence/var_inventory.captured.md \
  --trace-out spikes/dflowfm/evidence/single_reach.trace.txt
```

Once the runtime and companion files are stable, the same command shape can be
reused for `reach_with_weir.mdu`.

If the real BMI variable names differ, override `--boundary-var` and
`--stage-var` instead of recompiling the host.

## Expected outputs

### 1. Variable inventory

`--inventory-out` writes a markdown table containing:

- `index`
- `name`
- `type`
- `rank`
- `shape`
- `units`
- placeholder columns for `read/write role` and `notes`

Copy the captured table into `spikes/dflowfm/evidence/var_inventory.md` and
fill the final two columns.

### 2. Step trace

`--trace-out` writes:

- one header line with `t0`, `t1`, `engine_dt`, requested `steps`, and `dt`
- one header line describing columns: `step current_time stage0`
- one line per completed step
- one final summary line with:
  - `completed_steps`
  - `requested_steps`
  - `last_time`
  - `expected_last_time`
  - `max_dt_abs_error`
  - `time_trace_valid`

## Pass / fail reading

Treat the run as incomplete if ANY of the following are true:

- the host exits non-zero
- `completed_steps < requested_steps`
- `time_trace_valid=false`
- the trace file is missing
- the inventory file is missing

Do NOT promote G11 evidence from a partial run.

## After the run

Update these files together:

- `spikes/dflowfm/evidence/var_inventory.md`
- `spikes/dflowfm/evidence/spike_report.md`
- `spikes/dflowfm/evidence/interface_gap_matrix.md`
- optionally add a case-specific trace artifact path reference in the report

## Minimum evidence claim supported by one successful run

A successful run can support only these claims:

- the target D-Flow FM BMI library initialized and advanced for the requested
  step count
- real variable enumeration was captured
- a first-pass time-advance trace exists

It does NOT by itself justify:

- G11 `implemented`
- `ci_gate:true`
- G12 readiness
- replay / rollback parity claims

Those require the broader G11 plan entry criteria already documented in:

- `docs/superpowers/plans/2026-07-05-g11-dflowfm-river-steady-golden-plan.md`
- `spikes/dflowfm/evidence/spike_report.md`
