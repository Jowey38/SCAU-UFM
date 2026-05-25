# SWMM 5.2.4 interface gap matrix

Maps main-Spec §7.1 `ISwmmEngine` assumptions to real `swmm5.h` API.

Severity: `MATCH` | `MATCH_WITH_NOTE` | `GAP_MITIGABLE` | `GAP_BLOCKING` | `TBD`

| §7.1 assumption | Real API | Severity | Notes |
|---|---|---|---|
| Open input case | `swmm_open(f1, f2, f3)` | MATCH | rc=0 on success |
| Start simulation | `swmm_start(saveFlag)` | MATCH | |
| Step with caller-supplied dt | `swmm_step(double *elapsedTime)` | GAP_BLOCKING ? | `elapsedTime` is OUTPUT not INPUT; engine picks internal dt from `.inp` ROUTING_STEP. Mitigation candidate: `swmm_setValue(swmm_ROUTESTEP, ...)` before each step — verify in spike run. |
| Inject lateral inflow per node | `swmm_setValue(swmm_NODE_LATFLOW, idx, q)` | MATCH_WITH_NOTE | enum-based, no dedicated function; per spike-spec §5.3 expected |
| Read node head | `swmm_getValue(swmm_NODE_HEAD, idx)` | MATCH | |
| Read node overflow | `swmm_getValue(swmm_NODE_OVERFLOW, idx)` | MATCH | |
| Set outfall stage | `swmm_setValue(swmm_NODE_HEAD, outfall_idx, stage)` | MATCH_WITH_NOTE | only valid for outfall nodes |
| Save hot-start any step | NOT IN PUBLIC API | GAP_BLOCKING ? | `swmm5.c` has internal `hotstart_open`/`_close` but not exported. `.hsf` file produced by `swmm_start(saveFlag=1)` may only be readable on restart from beginning; verify in spike run |
| Restart from hot-start | `swmm_open` + `.inp` with HOTSTART line | MATCH_WITH_NOTE | restart only from saved checkpoint, not arbitrary mid-run |
| Same-process second instance | NOT POSSIBLE | GAP_BLOCKING | file-scope globals in `swmm5.c` preclude concurrent instances; documents for §6.5 snapshot scope |
| End / close | `swmm_end()`, `swmm_close()` | MATCH | |
| Error reporting | `swmm_getError(buf, len)` + return codes | MATCH | |
| Mass balance | `swmm_getMassBalErr(&runoff, &flow, &qual)` | MATCH | useful for §6.3 conservation cross-check |
| Object enumeration | `swmm_getCount(objType)`, `swmm_getName(objType, idx, ...)`, `swmm_getIndex(objType, name)` | MATCH | exhaustive object discovery available |
| System time accessors | `swmm_getValue(swmm_STARTDATE / swmm_CURRENTDATE / swmm_ELAPSEDTIME / ...)` | MATCH | enums 0..8 |

## Outstanding investigations for spike run

1. Verify whether `swmm_setValue(swmm_ROUTESTEP, ...)` mid-run is allowed and
   whether the engine actually respects the new value on the next `swmm_step`.
2. Locate any non-public hot-start save path (filename, format, lifecycle).
3. Confirm that `swmm_getError` returns the most recent error, not a cleared
   buffer, after rc!=0 from any API call.
4. Measure how `swmm_step` behaves when called from a process where another
   `swmm_open` has not yet been issued (defensive ABI behaviour).
