# D-Flow FM BMI 1.0 interface gap matrix

Maps main-Spec §7.3 / §7.4 `IDFlowFMEngine` assumptions to real BMI 1.0 API.

Severity: `MATCH` | `MATCH_WITH_NOTE` | `GAP_MITIGABLE` | `GAP_BLOCKING` | `TBD`

| §7.3-§7.4 assumption | Real API | Severity | Notes |
|---|---|---|---|
| Open with config file | `int initialize(const char *config_file)` | MATCH | |
| Step with caller-supplied dt | `int update(double dt)` | MATCH_WITH_NOTE | spike-spec §6.3 risk: verify engine actually respects caller dt (no silent internal multi-step) |
| Read state by name | `void get_var(const char *name, void **ptr)` | MATCH_WITH_NOTE | engine-owned pointer; lifetime tied to engine. Spike must lock down lifetime rules across `set_var` / `update` |
| Write state by name | `void set_var(const char *name, const void *ptr)` | MATCH | caller-owned buffer; engine copies |
| Save state mid-run | NOT IN BMI 1.0 | GAP_BLOCKING ? | investigate D-Flow FM non-BMI hot-start path |
| Multi-instance in process | NOT POSSIBLE | GAP_BLOCKING | free-function API implies single global state |
| RTC / weir / gate state read | NOT IN BMI 1.0 base | GAP_MITIGABLE ? | likely available via extra D-Flow FM-specific headers outside BMI; spike must locate (search under `src/engines_gpl/dflowfm/packages/dflowfm_kernel/`) |
| Variable enumeration | `get_var_count`, `get_var_name(i, char*)` | MATCH | useful for §7.5 `RiverExchangePoint` field discovery |
| Variable shape / rank / type | `get_var_shape`, `get_var_rank`, `get_var_type` | MATCH | dims up to 6 (`MAXDIMS`); type returned as type-name string |
| Logging | `set_logger(BMILogger)` | MATCH | function-pointer callback; install before `initialize` |
| Time accessors | `get_start_time` / `get_end_time` / `get_current_time` / `get_time_step` | MATCH | all out-parameter doubles |
| Finalize | `int finalize()` | MATCH | |
| Slice write | `void set_var_slice(name, start[], count[], ptr)` | MATCH_WITH_NOTE | useful when only part of an array needs updating; lifetime semantics same as `set_var` |

## Outstanding investigations for spike run

1. Confirm `update(dt)` honours caller-supplied `dt` exactly; instrument log
   ticks and compare against returned `get_current_time`.
2. Locate any D-Flow FM hot-start path outside BMI 1.0; document file format
   and lifecycle semantics.
3. Verify `get_var` pointer lifetime: does `set_var` invalidate previously
   returned pointers? does `update`?
4. Enumerate the full list of available variable names via
   `get_var_count` + `get_var_name`; record in `evidence/var_inventory.md`.
5. Confirm whether a second D-Flow FM DLL can be loaded into the same process
   under a different name; if not, document the §6.5 snapshot scope impact.
6. Locate RTC / weir / gate state read path; if outside BMI, document the
   extra ABI surface.
