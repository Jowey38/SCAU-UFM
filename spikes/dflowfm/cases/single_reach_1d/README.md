# SCAU-UFM authored single-reach D-Flow FM case

This case and its geometry are authored by the SCAU-UFM project. No Deltares
example, binary network, or test companion is copied into this directory.

Generate the UGRID network:

```bash
python tools/dflowfm/generate_single_reach_1d.py \
  spikes/dflowfm/cases/single_reach_1d/single_reach_net.nc \
  --netcdf-dll Delft3D-main/install_fm-suite_release/bin/netcdf.dll
```

The model is a straight 1000 m 1D reach with 11 mesh nodes, uniform width,
uniform bed level, and one zero-discharge lateral (`lat1`) at 500 m on
`branch_main`.
The initial validation stage intentionally uses a closed reach so UGRID/MDU
compatibility can be diagnosed independently from boundary forcing syntax.

Expected BMI checks:

- initialization succeeds;
- `s1` and `q1` are finite;
- `laterals/lat1/water_discharge` supports write/read/restore;
- 100 x `update(60)` reaches 6000 seconds exactly;
- native restart files are emitted every 600 seconds for later checkpoint tests.
