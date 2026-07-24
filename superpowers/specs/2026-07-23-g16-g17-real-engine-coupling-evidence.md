# G16/G17 real-engine coupling evidence

Date: 2026-07-23

Status: implemented, `ci_gate:false` (real D-Flow runtime is local, CI tests skip explicitly when absent).

## G16 dual_engine_shared_cell_real_both

Both real engines share one CouplingLib surface cell:

- real embedded SWMM 5.2.4;
- real runtime-loaded D-Flow FM release DLL;
- project endpoint 5 maps to `laterals/lat1/water_discharge`.

With `V_limit=36 m3`, priority 1:2, and competing requests of 4 m3/s:

- drainage `V_limit_k=12`, granted 12, unmet 4;
- river `V_limit_k=24`, granted 16, unmet 0;
- real SWMM receives/routes 3 m3/s;
- real D-Flow receives 4 m3/s at `lat1`;
- surface mass decreases by exactly 28 m3.

Local real-runtime result: passed.

## G17 tri_coupling_real_minimal

The authored D-Flow single reach and a project SWMM case use a common vertical
datum. Two real coupled substeps exercise:

1. surface -> real SWMM drainage and a zero surface -> river intent;
2. previous routed SWMM outfall inflow -> real D-Flow `lat1` through the
   1D-1D interface decision;
3. real D-Flow `s1` -> SWMM outfall stage backwater boundary;
4. both native engines advance by the shared 60 s substep;
5. no new surface request on substep 2, so surface mass is unchanged.

Assertions lock the interface granted flow/volume, native compound lateral
value, outfall stage, finite river stage, and 120 s D-Flow elapsed time.

Local real-runtime result: passed in 1.51 s.

## Boundary

These are non-gating until CI has a governed D-Flow FM runtime artifact or
runner. They do not claim arbitrary in-memory engine rollback; checkpoint
reload is the documented native recovery boundary.
