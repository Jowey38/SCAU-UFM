# D-Flow FM BMI 1.0 ABI gotchas

Initial findings from inspection of
`Delft3D-main/src/engines_gpl/dimr/packages/dimr_lib/include/bmi.h`.

## Thread safety

- TBD; BMI free-function style + likely Fortran-backed engine implies
  main-thread-only.

## Memory ownership

- `initialize` / `update` / `finalize`: int return code, no buffers.
- `set_var(name, const void *ptr)`: caller owns the buffer; engine copies.
- `get_var(name, void **ptr)`: engine writes its own pointer into `*ptr`.
  Caller MUST NOT free `*ptr`. Lifetime tied to engine; pointer may
  invalidate across `set_var` / `update`.
- `get_var_name(int, char*)`: caller-owned buffer; size = `MAXSTRINGLEN` (1024).
- `get_var_shape(name, int shape[MAXDIMS])`: caller-owned shape array, dims=6.
- `set_logger(BMILogger)`: caller-owned function pointer; engine retains it
  for the lifetime of the process.

## Units

- TBD; D-Flow FM BMI variable units are documented per variable name; spike
  must enumerate via `get_var_count` + `get_var_name` and look up in D-Flow FM
  user manual.

## Domain / sentinel values

- TBD.

## OS resources

- D-Flow FM is known to depend on the current working directory and on
  environment variables (per spike-spec §6.3 note). Spike must record actual
  dependencies in run.

## External deps

- D-Flow FM upstream depends on netCDF, MPI (optional), PETSc (optional), MKL,
  Fortran runtime. Spike must verify which are required at link time.

## Calling convention

- Windows: `__declspec(dllexport) __stdcall` via `BMI_API` and `BMI_CALLCONV`.
- Linux / macOS: default cdecl; no decoration.
- `BMILogger` callbacks installed via `set_logger` must use `BMI_CALLCONV`.
