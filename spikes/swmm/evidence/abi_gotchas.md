# SWMM 5.2.4 ABI gotchas

Initial findings from inspection of `swmm5.h` and `swmm5.c` from the
2026-05-24 working tree. Each section is `TBD by spike run` unless noted.

## Thread safety

- TBD (spike must run two threads each calling `swmm_step` in parallel and observe).
- Initial guess: NOT thread-safe; `swmm5.c` uses file-scope globals
  (`IsOpenFlag`, `Node[]`, `Link[]`, `Gage[]`, `Subcatch[]`).

## Memory ownership

- `swmm_getError(char *errMsg, int msgLen)`: caller-owned buffer; engine fills.
- `swmm_getName(int objType, int index, char *name, int size)`: caller-owned
  buffer; engine fills.
- `swmm_getValue`: returns `double` by value; no buffer.
- `swmm_setValue`: caller passes value by copy.
- No engine-owned pointers cross the ABI boundary.

## Units

- TBD; depends on `swmm_FLOWUNITS` system property (`CFS / GPM / MGD / CMS /
  LPS / MLD`).
- Head and depth are in feet for US units (CFS / GPM / MGD) and meters for SI
  units (CMS / LPS / MLD).
- Spike host must query `swmm_getValue(swmm_FLOWUNITS, 0)` after `swmm_open`
  and record the active unit set before reading any state field.

## Domain / sentinel values

- TBD by spike run.
- `swmm_getValue` on an invalid property/index combination returns 0 according
  to source inspection (silent failure). Spike must always validate index via
  `swmm_getIndex` before calling and treat 0 returns with caution.

## OS resources

- `swmm_open` opens 3 files (input / report / output); engine holds file
  handles until `swmm_close`.
- TBD: tmp file usage; hotstart `.hsf` location.

## External deps

- OpenMP (CMakeLists installs OpenMP runtime).
- No netCDF, no MKL in 5.2.4 base.
- TBD: any additional system libraries pulled in by SWMM-OpenMP build.

## Calling convention

- Windows: `__declspec(dllexport) __stdcall`. Spike host must match `__stdcall`
  via the public header (`extern "C"` block in `swmm5.h`).
- Linux / macOS: default cdecl; no decoration.
