# D-Flow FM BMI 1.0 interface gap matrix

Maps main-Spec §7.3 / §7.4 `IDFlowFMEngine` assumptions to real BMI 1.0 API.

Severity: `MATCH` | `MATCH_WITH_NOTE` | `GAP_MITIGABLE` | `GAP_BLOCKING` | `TBD`

| §7.3-§7.4 assumption | Real API | Severity | Notes |
|---|---|---|---|
| Open with config file | `int initialize(const char *config_file)` | MATCH | |
| Step with caller-supplied dt | `int update(double dt)` | MATCH | 2026-07-19 release run: 100 x `update(60)` produced exact 60 s increments; max absolute error 0 |
| Read state by name | `void get_var(const char *name, void **ptr)` | MATCH_WITH_NOTE | main-graph adapter treats the returned pointer as engine-owned double storage; spike must still lock down lifetime rules across `set_var` / `update` |
| Write state by name | `void set_var(const char *name, const void *ptr)` | MATCH | 2026-07-19 real pass: compound path `laterals/lat1/water_discharge` accepted double write/read/restore (0 -> 0.125 -> 0); 2D uses one value per lateral |
| Save state mid-run | NOT IN BMI 1.0 | GAP_BLOCKING | investigate D-Flow FM non-BMI hot-start path before claiming replay/rollback parity |
| Multi-instance in process | NOT POSSIBLE | GAP_BLOCKING | free-function API implies single global state; main-graph adapter enforces one initialized `DFlowFMEngine` per process |
| RTC / weir / gate state read | NOT IN BMI 1.0 base | GAP_MITIGABLE | likely available via extra D-Flow FM-specific headers outside BMI; spike must locate (search under `src/engines_gpl/dflowfm/packages/dflowfm_kernel/`) |
| Variable enumeration | `get_var_count`, `get_var_name(i, char*)` | MATCH | 195 variables captured; `s1` water level and `q1` discharge confirmed |
| Variable shape / rank / type | `get_var_shape`, `get_var_rank`, `get_var_type` | GAP_MITIGABLE | rank/type work for 195 variables; release DLL access-violates in `get_var_shape` for some rank>1 variables (`bodsed` first observed). Host queries shape only for rank<=1 |
| Logging | `set_logger(BMILogger)` | MATCH | function-pointer callback; install before `initialize` |
| Time accessors | `get_start_time` / `get_end_time` / `get_current_time` / `get_time_step` | MATCH | all out-parameter doubles |
| Finalize | `int finalize()` | MATCH | |
| Slice write | `void set_var_slice(name, start[], count[], ptr)` | MATCH_WITH_NOTE | generated numeric arrays support zero-based slices, but compound `laterals` has no top-level slice implementation; lateral exchange must use `set_var("laterals/<id>/water_discharge", ptr)` |

## Outstanding investigations for spike run

1. Locate any D-Flow FM hot-start path outside BMI 1.0; document file format
   and lifecycle semantics.
2. Verify `get_var` pointer lifetime across `update`; write/read/restore across
   `set_var` is confirmed for the lateral compound field.
3. Confirm whether a second D-Flow FM DLL can be loaded into the same process
   under a different name; if not, document the section 6.5 snapshot scope impact.
4. Locate RTC / weir / gate state read path; if outside BMI, document the
   extra ABI surface.
5. Replace the local Deltares-derived Flow1D assets with a repository-authored
   redistributable single-reach case before promoting G11 to a CI gate.
