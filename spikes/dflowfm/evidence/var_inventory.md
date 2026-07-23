# D-Flow FM BMI 1.0 variable inventory

Status: real runtime captured on 2026-07-19 from the release build at
`Delft3D-main/install_fm-suite_release/bin/dflowfm.dll` using the local
`Flow1D.mdu` companion set. Full 195-variable machine capture:
`var_inventory.captured.md`.

## Runtime identity and capture

- `DFLOWFM_SOURCE_DIR`: `H:/githubcode/SCAU-UFM/Delft3D-main`
- release DLL: `install_fm-suite_release/bin/dflowfm.dll`
- debug DLL: `install_fm-suite/bin/dflowfm.dll`
- case: `spikes/dflowfm/cases/single_reach/Flow1D.mdu` with
  `Flow1D_net.nc`, `IniCondition.ini`, and `initialfields.ini`
- variable count: **195**
- BMI header/version: 1.0
- units: unavailable through BMI 1.0 (there is no `get_var_units` symbol)

## G11 required highlights

| index | runtime name | type | rank | shape | role | conclusion |
|---|---|---|---|---|---|---|
| 90 | `s1` | double | 1 | 186 | water level / stage read | confirmed; used for the successful 100-step trace |
| 91 | `s1max` | double | 1 | 186 | maximum water level read | confirmed metadata |
| 97 | `hs` | double | 1 | 186 | water depth read | confirmed metadata |
| 112 | `q1` | double | 1 | 190 | link discharge read | confirmed metadata |
| 113 | `q1_main` | double | 1 | 190 | main discharge read | confirmed metadata |
| 182 | `laterals` | compound (field `water_discharge` is double) | 2 | `[1,1]` in the one-lateral case | lateral exchange write/read | confirmed through `laterals/lat1/water_discharge`; write 0.125, read back, restore 0, read back |

## ABI findings

1. `get_var_count`, `get_var_name`, `get_var_type`, and `get_var_rank` work for
   the complete 195-variable inventory.
2. `get_var_shape` works for rank 0/1 variables but the release DLL raises an
   access violation for some rank>1 variables (first reproduced on `bodsed`,
   index 5, rank 2). The spike host now records high-rank shapes as unavailable
   rather than invoking the crashing ABI path.
3. BMI 1.0 does not export `get_var_units`; the inventory records
   `N/A (BMI 1.0)` instead of assuming units.
4. Project placeholder variable names `water_level`, `stage_at_section`, and
   `boundary_discharge` are not the real names for this build/case. Configure
   stage reads as `s1`; discharge reads as `q1`/`q1_main` as appropriate.
5. `laterals` is a compound BMI object, not a normal rank-2 numeric array.
   The safe write path is `set_var("laterals/<id>/water_discharge", ptr)`, not
   `set_var_slice("laterals", ...)`. In a derived one-lateral case,
   `get_var_shape("laterals")` returned `[1,1,0,0,0,0]`; writing 0.125 to
   `laterals/lat1/water_discharge`, reading it back, restoring 0, and reading
   the restored value all succeeded. The field is `double`, and 2D models use
   one value per lateral (m3/s).

## G11 consequence

The real runtime now satisfies lifecycle, enumeration, rank-1 stage read,
compound lateral write/read/restore, and 100-step time-advance evidence. Full
G11 promotion remains blocked on a repository-authored redistributable
single-reach case and the separately documented BMI 1.0 hot-start/replay gap;
the current Flow1D companions are local Deltares test assets and remain
untracked.
