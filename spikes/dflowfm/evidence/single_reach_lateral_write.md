# Real D-Flow FM compound lateral write evidence

Date: 2026-07-19

Runtime: `Delft3D-main/install_fm-suite_release/bin/dflowfm.dll`

Local case construction: `spikes/dflowfm/cases/README_single_reach_lateral.md`
(the Deltares-derived `.mdu` / network companions remain gitignored).

## Compound metadata

With one registered lateral (`id=lat1`):

```text
get_var_shape("laterals") = [1, 1, 0, 0, 0, 0]
```

Source tracing confirms that `laterals` is a compound BMI object. Its writable
2D water-discharge field is addressed by path:

```text
laterals/lat1/water_discharge
```

The top-level `set_var_slice("laterals", ...)` API is not the applicable path.

## Write/read/restore result

The spike host read the initial value, wrote a bounded probe, read it back,
restored the original value, and read the restoration:

```text
lateral_write_restore_valid=true variable=laterals/lat1/water_discharge before=0 probe=0.125 restored=0
```

The value type is `double`; for this 2D model there is one value per lateral,
with physical units m3/s according to the D-Flow FM source declaration.

## Post-restore time advance

After restoration, the host completed 100 calls to `update(60)`:

```text
completed_steps=100
requested_steps=100
last_time=6000
expected_last_time=6000
max_dt_abs_error=0
time_trace_valid=true
```

Full trace: `single_reach_lateral.trace.txt`.

## Boundary of claim

This evidence validates the real BMI compound lateral write contract and exact
time advance after restoring the original value. It does not yet provide a
repository-redistributable hydraulic lateral-forcing Golden case, and it does
not resolve the BMI 1.0 hot-start/save-state gap.
