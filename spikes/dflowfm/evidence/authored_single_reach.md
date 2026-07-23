# SCAU-UFM authored D-Flow FM single-reach evidence

Date: 2026-07-23

## Provenance

The MDU, external forcing file, straight-reach geometry, and NetCDF generator
are authored by SCAU-UFM. No Deltares example/test network or companion asset
is included.

Generator:

`tools/dflowfm/generate_single_reach_1d.py`

Generated network SHA-256:

`71efdc09db8d6fa9c04798350a4df0430b2e76de9a2722b6f1eb0af74264a687`

Regeneration is byte-identical with the same NetCDF runtime.

## Model

- one straight branch, 1000 m;
- 11 mesh nodes / 10 mesh edges;
- uniform width 5 m, initial water level 1 m;
- one compound lateral `lat1` at chainage 500 m;
- simulation period 6000 s;
- restart interval 600 s.

## Real runtime result

Release DLL:

`Delft3D-main/install_fm-suite_release/bin/dflowfm.dll`

Result:

```text
Succesfully added lateral lat1.
lateral_write_restore_valid=true variable=laterals/lat1/water_discharge before=0 probe=0.125 restored=0
completed_steps=100 requested_steps=100 last_time=6000 expected_last_time=6000 max_dt_abs_error=0 time_trace_valid=true
```

The run produced native `*_rst.nc` checkpoint files at the configured 600 s
interval, including time-zero and subsequent checkpoints.

Full trace: `authored_single_reach.trace.txt`.

## Remaining scope

The first authored case is intentionally closed/zero-forcing to isolate UGRID,
lifecycle, state-read, compound lateral write, time advance, and restart-output
compatibility. A later physical Golden may add authored upstream discharge and
downstream stage boundaries after the native mapping is wired into the driver.
