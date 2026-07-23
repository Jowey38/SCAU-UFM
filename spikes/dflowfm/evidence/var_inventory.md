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
| 182 | `laterals` | type string empty | 2 | unavailable | lateral exchange candidate | `get_var_shape` crashes in this runtime for some rank>1 variables; do not write until `set_var_slice` evidence exists |

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
5. A safe lateral write variable has not yet been established. `laterals` is
   rank 2 and requires shape/index semantics plus `set_var_slice` evidence.

## G11 consequence

The real runtime now satisfies lifecycle, enumeration, rank-1 stage read, and
100-step time-advance evidence. Full bidirectional G11 promotion remains
blocked only on a validated lateral/boundary write mapping and a repository-
authored redistributable single-reach case (the current Flow1D companions are
local Deltares test assets and remain gitignored).
